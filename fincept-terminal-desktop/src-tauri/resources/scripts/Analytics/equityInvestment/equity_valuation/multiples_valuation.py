"""
Equity Investment Multiples Valuation Module
======================================

Relative valuation using multiples

===== DATA SOURCES REQUIRED =====
INPUT:
  - Company financial statements and SEC filings
  - Market price data and trading volume information
  - Industry reports and competitive analysis data
  - Management guidance and analyst estimates
  - Economic indicators affecting equity markets

OUTPUT:
  - Equity valuation models and fair value estimates
  - Fundamental analysis metrics and financial ratios
  - Investment recommendations and target prices
  - Risk assessments and portfolio implications
  - Sector and industry comparative analysis

PARAMETERS:
  - valuation_method: Primary valuation methodology (default: 'DCF')
  - discount_rate: Discount rate for valuation (default: 0.10)
  - terminal_growth: Terminal growth rate assumption (default: 0.025)
  - earnings_multiple: Target earnings multiple (default: 15.0)
  - reporting_currency: Reporting currency (default: 'USD')
"""

import numpy as np
import pandas as pd
from typing import List, Dict, Any, Optional, Tuple, Union
from dataclasses import dataclass
import statistics
from scipy import stats

from ..base.base_models import (
    BaseValuationModel, CompanyData, MarketData, ValuationResult,
    ValuationMethod, CalculationEngine, ModelValidator, ValidationError
)


@dataclass
class ComparableCompany:
    """Data structure for comparable company"""
    symbol: str
    name: str
    sector: str
    market_cap: float
    enterprise_value: float
    revenue: float
    ebitda: float
    net_income: float
    book_value: float
    current_price: float
    multiples: Dict[str, float]


class PriceMultiplesModel(BaseValuationModel):
    """Price-based multiple valuation model"""

    def __init__(self):
        super().__init__("Price Multiples", "Price-based multiple valuation")
        self.valuation_method = ValuationMethod.MULTIPLES_PE

    def calculate_pe_ratio(self, price: float, eps: float) -> float:
        """Calculate Price-to-Earnings ratio"""
        if eps <= 0:
            raise ValidationError("Earnings per share must be positive for P/E calculation")
        return price / eps

    def calculate_pb_ratio(self, price: float, book_value_per_share: float) -> float:
        """Calculate Price-to-Book ratio"""
        if book_value_per_share <= 0:
            raise ValidationError("Book value per share must be positive for P/B calculation")
        return price / book_value_per_share

    def calculate_ps_ratio(self, price: float, sales_per_share: float) -> float:
        """Calculate Price-to-Sales ratio"""
        if sales_per_share <= 0:
            raise ValidationError("Sales per share must be positive for P/S calculation")
        return price / sales_per_share

    def calculate_peg_ratio(self, pe_ratio: float, growth_rate: float) -> float:
        """Calculate PEG ratio"""
        if growth_rate <= 0:
            raise ValidationError("Growth rate must be positive for PEG calculation")
        return pe_ratio / (growth_rate * 100)  # Convert growth to percentage

    def calculate_dividend_yield(self, dividend_per_share: float, price: float) -> float:
        """Calculate dividend yield"""
        if price <= 0:
            raise ValidationError("Price must be positive for dividend yield calculation")
        return dividend_per_share / price

    def calculate_earnings_yield(self, eps: float, price: float) -> float:
        """Calculate earnings yield (E/P ratio)"""
        if price <= 0:
            raise ValidationError("Price must be positive for earnings yield calculation")
        return eps / price

    def normalize_earnings(self, earnings_history: List[float], method: str = "average") -> float:
        """Normalize earnings using various methods"""
        if not earnings_history:
            raise ValidationError("Earnings history cannot be empty")

        # Remove negative or zero earnings for certain methods
        positive_earnings = [e for e in earnings_history if e > 0]

        if method == "average":
            return statistics.mean(earnings_history)
        elif method == "median":
            return statistics.median(earnings_history)
        elif method == "average_positive":
            return statistics.mean(positive_earnings) if positive_earnings else 0
        elif method == "peak_earnings":
            return max(earnings_history)
        elif method == "trough_earnings":
            return min(positive_earnings) if positive_earnings else 0
        else:
            raise ValidationError(f"Unknown normalization method: {method}")

    def calculate_justified_pe_from_fundamentals(self, payout_ratio: float, required_return: float,
                                                 growth_rate: float, is_leading: bool = True) -> float:
        """Calculate justified P/E ratio from fundamentals"""
        justified_pe = CalculationEngine.pe_ratio_from_fundamentals(payout_ratio, required_return, growth_rate)

        if not is_leading:
            # Convert to trailing P/E
            justified_pe = justified_pe / (1 + growth_rate)

        return justified_pe

    def calculate_justified_pb_from_fundamentals(self, roe: float, required_return: float,
                                                 growth_rate: float) -> float:
        """Calculate justified P/B ratio from fundamentals"""
        if required_return <= growth_rate:
            raise ValidationError("Required return must be greater than growth rate")

        return (roe - growth_rate) / (required_return - growth_rate)

    def calculate_justified_ps_from_fundamentals(self, profit_margin: float, payout_ratio: float,
                                                 required_return: float, growth_rate: float) -> float:
        """Calculate justified P/S ratio from fundamentals"""
        justified_pe = self.calculate_justified_pe_from_fundamentals(payout_ratio, required_return, growth_rate)
        return justified_pe * profit_margin

    def value_using_pe_multiple(self, comparable_pe: float, target_eps: float) -> float:
        """Value company using P/E multiple"""
        return comparable_pe * target_eps

    def value_using_pb_multiple(self, comparable_pb: float, target_bvps: float) -> float:
        """Value company using P/B multiple"""
        return comparable_pb * target_bvps

    def value_using_ps_multiple(self, comparable_ps: float, target_sps: float) -> float:
        """Value company using P/S multiple"""
        return comparable_ps * target_sps


class EnterpriseValueMultiplesModel(BaseValuationModel):
    """Enterprise Value multiple valuation model"""

    def __init__(self):
        super().__init__("EV Multiples", "Enterprise value multiple valuation")
        self.valuation_method = ValuationMethod.MULTIPLES_EV_EBITDA

    def calculate_enterprise_value(self, market_cap: float, total_debt: float,
                                   cash: float, preferred_stock: float = 0) -> float:
        """Calculate Enterprise Value"""
        return market_cap + total_debt - cash + preferred_stock

    def calculate_ev_ebitda(self, enterprise_value: float, ebitda: float) -> float:
        """Calculate EV/EBITDA multiple"""
        if ebitda <= 0:
            raise ValidationError("EBITDA must be positive for EV/EBITDA calculation")
        return enterprise_value / ebitda

    def calculate_ev_sales(self, enterprise_value: float, sales: float) -> float:
        """Calculate EV/Sales multiple"""
        if sales <= 0:
            raise ValidationError("Sales must be positive for EV/Sales calculation")
        return enterprise_value / sales

    def calculate_ev_ebit(self, enterprise_value: float, ebit: float) -> float:
        """Calculate EV/EBIT multiple"""
        if ebit <= 0:
            raise ValidationError("EBIT must be positive for EV/EBIT calculation")
        return enterprise_value / ebit

    def calculate_ev_fcf(self, enterprise_value: float, free_cash_flow: float) -> float:
        """Calculate EV/FCF multiple"""
        if free_cash_flow <= 0:
            raise ValidationError("Free cash flow must be positive for EV/FCF calculation")
        return enterprise_value / free_cash_flow

    def value_using_ev_multiple(self, comparable_ev_multiple: float, target_metric: float,
                                target_debt: float, target_cash: float,
                                target_shares: float, preferred_stock: float = 0) -> float:
        """Value company using EV multiple and convert to per-share value"""
        # Calculate implied enterprise value
        implied_ev = comparable_ev_multiple * target_metric

        # Convert to equity value
        equity_value = implied_ev - target_debt + target_cash - preferred_stock

        # Convert to per-share value
        return equity_value / target_shares if target_shares > 0 else 0


class ComparablesAnalyzer:
    """Analyze and select comparable companies"""

    def __init__(self):
        self.price_model = PriceMultiplesModel()
        self.ev_model = EnterpriseValueMultiplesModel()

    def calculate_all_multiples(self, company_data: ComparableCompany) -> Dict[str, float]:
        """Calculate all relevant multiples for a company"""
        multiples = {}

        try:
            # Price multiples
            if company_data.net_income > 0:
                eps = company_data.net_income / (company_data.market_cap / company_data.current_price)
                multiples['pe_ratio'] = self.price_model.calculate_pe_ratio(company_data.current_price, eps)
                multiples['earnings_yield'] = self.price_model.calculate_earnings_yield(eps, company_data.current_price)

            if company_data.book_value > 0:
                bvps = company_data.book_value / (company_data.market_cap / company_data.current_price)
                multiples['pb_ratio'] = self.price_model.calculate_pb_ratio(company_data.current_price, bvps)

            if company_data.revenue > 0:
                sps = company_data.revenue / (company_data.market_cap / company_data.current_price)
                multiples['ps_ratio'] = self.price_model.calculate_ps_ratio(company_data.current_price, sps)

            # Enterprise value multiples
            if company_data.ebitda > 0:
                multiples['ev_ebitda'] = self.ev_model.calculate_ev_ebitda(company_data.enterprise_value,
                                                                           company_data.ebitda)

            if company_data.revenue > 0:
                multiples['ev_sales'] = self.ev_model.calculate_ev_sales(company_data.enterprise_value,
                                                                         company_data.revenue)

        except ValidationError:
            pass  # Skip ratios that can't be calculated

        return multiples

    def screen_comparables(self, comparables: List[ComparableCompany],
                           target_company: ComparableCompany,
                           screening_criteria: Dict[str, Any]) -> List[ComparableCompany]:
        """Screen comparable companies based on criteria"""
        screened = []

        for comp in comparables:
            # Sector screening
            if screening_criteria.get('same_sector', True) and comp.sector != target_company.sector:
                continue

            # Size screening
            size_ratio_limit = screening_criteria.get('max_size_ratio', 10)
            size_ratio = max(comp.market_cap, target_company.market_cap) / min(comp.market_cap,
                                                                               target_company.market_cap)
            if size_ratio > size_ratio_limit:
                continue

            # Profitability screening
            min_roe = screening_criteria.get('min_roe', -0.5)
            comp_roe = comp.net_income / comp.book_value if comp.book_value > 0 else -1
            if comp_roe < min_roe:
                continue

            # Growth screening
            if 'min_growth' in screening_criteria:
                # Would need historical data for growth calculation
                pass

            screened.append(comp)

        return screened

    def calculate_multiple_statistics(self, comparables: List[ComparableCompany],
                                      multiple_type: str) -> Dict[str, float]:
        """Calculate statistics for a specific multiple across comparables"""

        multiples_values = []
        for comp in comparables:
            comp_multiples = self.calculate_all_multiples(comp)
            if multiple_type in comp_multiples and comp_multiples[multiple_type] > 0:
                multiples_values.append(comp_multiples[multiple_type])

        if not multiples_values:
            raise ValidationError(f"No valid {multiple_type} values found in comparables")

        return {
            'mean': statistics.mean(multiples_values),
            'median': statistics.median(multiples_values),
            'harmonic_mean': statistics.harmonic_mean(multiples_values),
            'weighted_harmonic_mean': self._calculate_weighted_harmonic_mean(multiples_values, comparables),
            'min': min(multiples_values),
            'max': max(multiples_values),
            'std_dev': statistics.stdev(multiples_values) if len(multiples_values) > 1 else 0,
            'count': len(multiples_values),
            'values': multiples_values
        }

    def _calculate_weighted_harmonic_mean(self, multiples: List[float],
                                          comparables: List[ComparableCompany]) -> float:
        """Calculate weighted harmonic mean using market cap as weights"""
        # Simplified - would need proper weights based on the specific multiple
        weights = [comp.market_cap for comp in comparables[:len(multiples)]]
        total_weight = sum(weights)

        weighted_sum = sum(w / m for w, m in zip(weights, multiples))
        return total_weight / weighted_sum


class CrossSectionalRegressionAnalyzer:
    """Cross-sectional regression analysis for multiples"""

    def predict_pe_ratio(self, fundamentals_data: pd.DataFrame,
                         target_fundamentals: Dict[str, float]) -> Dict[str, Any]:
        """Predict P/E ratio using cross-sectional regression on fundamentals"""

        # Prepare regression variables
        X = fundamentals_data[['growth_rate', 'payout_ratio', 'beta', 'roe']].fillna(0)
        y = fundamentals_data['pe_ratio'].fillna(0)

        # Remove outliers (P/E > 50 or < 0)
        mask = (y > 0) & (y < 50)
        X = X[mask]
        y = y[mask]

        if len(X) < 3:
            raise ValidationError("Insufficient data for regression analysis")

        # Perform regression
        from sklearn.linear_model import LinearRegression
        from sklearn.metrics import r2_score

        model = LinearRegression()
        model.fit(X, y)

        # Predict for target company
        target_X = np.array([[
            target_fundamentals.get('growth_rate', 0),
            target_fundamentals.get('payout_ratio', 0),
            target_fundamentals.get('beta', 1),
            target_fundamentals.get('roe', 0)
        ]])

        predicted_pe = model.predict(target_X)[0]

        # Calculate regression statistics
        y_pred = model.predict(X)
        r_squared = r2_score(y, y_pred)

        return {
            'predicted_pe': predicted_pe,
            'coefficients': dict(zip(['growth_rate', 'payout_ratio', 'beta', 'roe'], model.coef_)),
            'intercept': model.intercept_,
            'r_squared': r_squared,
            'sample_size': len(X),
            'target_fundamentals': target_fundamentals
        }


class MultiplesValuationSuite:
    """Comprehensive multiples valuation analysis"""

    def __init__(self):
        self.price_model = PriceMultiplesModel()
        self.ev_model = EnterpriseValueMultiplesModel()
        self.comparables_analyzer = ComparablesAnalyzer()
        self.regression_analyzer = CrossSectionalRegressionAnalyzer()

    def comprehensive_multiples_valuation(self, target_company: CompanyData,
                                          comparables: List[ComparableCompany],
                                          multiples_to_use: List[str] = None) -> Dict[str, ValuationResult]:
        """Perform comprehensive multiples valuation"""

        if multiples_to_use is None:
            multiples_to_use = ['pe_ratio', 'pb_ratio', 'ps_ratio', 'ev_ebitda', 'ev_sales']

        results = {}

        # Convert target company to comparable format
        target_comparable = self._convert_to_comparable(target_company)

        for multiple_type in multiples_to_use:
            try:
                # Calculate multiple statistics
                stats = self.comparables_analyzer.calculate_multiple_statistics(comparables, multiple_type)

                # Use median as the representative multiple (most robust)
                representative_multiple = stats['median']

                # Calculate valuation based on multiple type
                if multiple_type in ['pe_ratio', 'pb_ratio', 'ps_ratio']:
                    intrinsic_value = self._calculate_price_multiple_value(
                        target_company, multiple_type, representative_multiple
                    )
                else:  # EV multiples
                    intrinsic_value = self._calculate_ev_multiple_value(
                        target_company, multiple_type, representative_multiple
                    )

                # Create valuation result
                assumptions = {
                    'multiple_type': multiple_type,
                    'representative_multiple': representative_multiple,
                    'comparables_count': stats['count'],
                    'multiple_range': f"{stats['min']:.2f} - {stats['max']:.2f}",
                    'multiple_std_dev': stats['std_dev'],
                    'method': 'Median of Comparables'
                }

                calculation_details = {
                    'multiple_statistics': stats,
                    'target_metric': self._get_target_metric(target_company, multiple_type),
                    'calculation': f"{representative_multiple:.2f} × {self._get_target_metric(target_company, multiple_type):.2f}"
                }

                recommendation = self.price_model.generate_recommendation(intrinsic_value, target_company.current_price)
                upside_downside = self.price_model.calculate_upside_downside(intrinsic_value,
                                                                             target_company.current_price)

                results[multiple_type] = ValuationResult(
                    method=ValuationMethod.MULTIPLES_PE,  # Generic multiples method
                    intrinsic_value=intrinsic_value,
                    current_price=target_company.current_price,
                    recommendation=recommendation,
                    upside_downside=upside_downside,
                    confidence_level="MEDIUM",
                    assumptions=assumptions,
                    calculation_details=calculation_details
                )

            except Exception as e:
                results[multiple_type] = f"Error: {str(e)}"

        return results

    def _convert_to_comparable(self, company_data: CompanyData) -> ComparableCompany:
        """Convert CompanyData to ComparableCompany format"""
        financial_data = company_data.financial_data

        return ComparableCompany(
            symbol=company_data.symbol,
            name=company_data.name,
            sector=company_data.sector,
            market_cap=company_data.market_cap,
            enterprise_value=self.ev_model.calculate_enterprise_value(
                company_data.market_cap,
                financial_data.get('total_debt', 0),
                financial_data.get('cash', 0)
            ),
            revenue=financial_data.get('revenue', 0),
            ebitda=financial_data.get('ebitda', 0),
            net_income=financial_data.get('net_income', 0),
            book_value=financial_data.get('book_value', 0) * company_data.shares_outstanding,
            current_price=company_data.current_price,
            multiples={}
        )

    def _calculate_price_multiple_value(self, company_data: CompanyData,
                                        multiple_type: str, multiple_value: float) -> float:
        """Calculate value using price multiples"""
        financial_data = company_data.financial_data
        shares = company_data.shares_outstanding

        if multiple_type == 'pe_ratio':
            eps = financial_data.get('earnings_per_share', 0)
            return self.price_model.value_using_pe_multiple(multiple_value, eps)

        elif multiple_type == 'pb_ratio':
            book_value_total = financial_data.get('book_value', 0) * shares
            bvps = book_value_total / shares if shares > 0 else 0
            return self.price_model.value_using_pb_multiple(multiple_value, bvps)

        elif multiple_type == 'ps_ratio':
            revenue = financial_data.get('revenue', 0)
            sps = revenue / shares if shares > 0 else 0
            return self.price_model.value_using_ps_multiple(multiple_value, sps)

        else:
            raise ValidationError(f"Unknown price multiple type: {multiple_type}")

    def _calculate_ev_multiple_value(self, company_data: CompanyData,
                                     multiple_type: str, multiple_value: float) -> float:
        """Calculate value using EV multiples"""
        financial_data = company_data.financial_data

        if multiple_type == 'ev_ebitda':
            target_metric = financial_data.get('ebitda', 0)
        elif multiple_type == 'ev_sales':
            target_metric = financial_data.get('revenue', 0)
        else:
            raise ValidationError(f"Unknown EV multiple type: {multiple_type}")

        return self.ev_model.value_using_ev_multiple(
            multiple_value, target_metric,
            financial_data.get('total_debt', 0),
            financial_data.get('cash', 0),
            company_data.shares_outstanding
        )

    def _get_target_metric(self, company_data: CompanyData, multiple_type: str) -> float:
        """Get the target metric value for multiple calculation"""
        financial_data = company_data.financial_data

        if multiple_type == 'pe_ratio':
            return financial_data.get('earnings_per_share', 0)
        elif multiple_type == 'pb_ratio':
            return financial_data.get('book_value', 0)
        elif multiple_type == 'ps_ratio':
            return financial_data.get('revenue', 0) / company_data.shares_outstanding
        elif multiple_type == 'ev_ebitda':
            return financial_data.get('ebitda', 0)
        elif multiple_type == 'ev_sales':
            return financial_data.get('revenue', 0)
        else:
            return 0


# Convenience functions
def pe_multiple_valuation(target_eps: float, comparable_pe: float) -> float:
    """Quick P/E multiple valuation"""
    model = PriceMultiplesModel()
    return model.value_using_pe_multiple(comparable_pe, target_eps)


def ev_ebitda_valuation(target_ebitda: float, comparable_ev_ebitda: float,
                        debt: float, cash: float, shares: float) -> float:
    """Quick EV/EBITDA valuation"""
    model = EnterpriseValueMultiplesModel()
    return model.value_using_ev_multiple(comparable_ev_ebitda, target_ebitda, debt, cash, shares)