
"""Equity Investment Validators Module
======================================

Data validation and quality checks

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



import pandas as pd
import numpy as np
from typing import Dict, Any, List, Union, Optional
from datetime import datetime
import re

from .base_models import (
    CompanyData, MarketData, ValidationError,
    ValuationMethod, SecurityType
)


class CFAValidator:
    """Comprehensive validator based on CFA curriculum standards"""

    @staticmethod
    def validate_financial_ratios(ratios: Dict[str, float]) -> Dict[str, List[str]]:
        """Validate financial ratios and return warnings/errors"""
        warnings = []
        errors = []

        # P/E ratio validation
        if 'pe_ratio' in ratios:
            pe = ratios['pe_ratio']
            if pe < 0:
                errors.append("P/E ratio cannot be negative (company has negative earnings)")
            elif pe > 100:
                warnings.append(f"P/E ratio of {pe:.2f} is unusually high - verify earnings quality")

        # P/B ratio validation
        if 'pb_ratio' in ratios:
            pb = ratios['pb_ratio']
            if pb < 0:
                errors.append("P/B ratio cannot be negative (negative book value)")
            elif pb > 10:
                warnings.append(f"P/B ratio of {pb:.2f} is very high - company may be overvalued or asset-light")

        # ROE validation
        if 'roe' in ratios:
            roe = ratios['roe']
            if roe < -0.5:
                warnings.append(f"ROE of {roe:.2%} indicates significant losses")
            elif roe > 0.5:
                warnings.append(f"ROE of {roe:.2%} is exceptionally high - verify sustainability")

        # Debt-to-Equity validation
        if 'debt_to_equity' in ratios:
            de = ratios['debt_to_equity']
            if de < 0:
                errors.append("Debt-to-Equity ratio cannot be negative")
            elif de > 5:
                warnings.append(f"D/E ratio of {de:.2f} indicates high leverage - assess financial risk")

        # Current ratio validation
        if 'current_ratio' in ratios:
            cr = ratios['current_ratio']
            if cr < 1:
                warnings.append(f"Current ratio of {cr:.2f} may indicate liquidity concerns")
            elif cr > 5:
                warnings.append(f"Current ratio of {cr:.2f} may indicate inefficient asset utilization")

        return {'warnings': warnings, 'errors': errors}

    @staticmethod
    def validate_growth_rates(growth_data: Dict[str, float]) -> Dict[str, List[str]]:
        """Validate growth rate assumptions"""
        warnings = []
        errors = []

        # Revenue growth validation
        if 'revenue_growth' in growth_data:
            rg = growth_data['revenue_growth']
            if rg < -0.5:
                warnings.append(f"Revenue decline of {rg:.2%} is severe - verify business viability")
            elif rg > 0.5:
                warnings.append(f"Revenue growth of {rg:.2%} is very high - assess sustainability")

        # Earnings growth validation
        if 'earnings_growth' in growth_data:
            eg = growth_data['earnings_growth']
            if eg > 1.0:
                warnings.append(f"Earnings growth of {eg:.2%} is extremely high - verify quality")

        # Long-term growth validation
        if 'long_term_growth' in growth_data:
            ltg = growth_data['long_term_growth']
            if ltg > 0.06:  # 6% long-term growth is generally considered maximum sustainable
                warnings.append(f"Long-term growth of {ltg:.2%} exceeds typical economic growth limits")
            elif ltg < 0:
                warnings.append("Negative long-term growth assumptions should be carefully justified")

        return {'warnings': warnings, 'errors': errors}

    @staticmethod
    def validate_discount_rates(rates: Dict[str, float]) -> Dict[str, List[str]]:
        """Validate discount rate assumptions"""
        warnings = []
        errors = []

        # Risk-free rate validation
        if 'risk_free_rate' in rates:
            rf = rates['risk_free_rate']
            if rf < 0:
                warnings.append("Negative risk-free rate - unusual market conditions")
            elif rf > 0.15:
                warnings.append(f"Risk-free rate of {rf:.2%} is very high - verify source")

        # Required return validation
        if 'required_return' in rates:
            rr = rates['required_return']
            if rr < 0.02:
                warnings.append(f"Required return of {rr:.2%} seems too low")
            elif rr > 0.25:
                warnings.append(f"Required return of {rr:.2%} is very high - verify risk assessment")

        # WACC validation
        if 'wacc' in rates:
            wacc = rates['wacc']
            if wacc < 0.03:
                warnings.append(f"WACC of {wacc:.2%} seems low - verify calculation")
            elif wacc > 0.20:
                warnings.append(f"WACC of {wacc:.2%} is high - assess company risk")

        # Risk premium validation
        if 'risk_free_rate' in rates and 'required_return' in rates:
            risk_premium = rates['required_return'] - rates['risk_free_rate']
            if risk_premium < 0:
                errors.append("Risk premium cannot be negative")
            elif risk_premium > 0.15:
                warnings.append(f"Risk premium of {risk_premium:.2%} is very high")

        return {'warnings': warnings, 'errors': errors}


class DDMValidator:
    """Validator for Dividend Discount Models"""

    @staticmethod
    def validate_gordon_growth_inputs(dividend: float, growth_rate: float,
                                      required_return: float) -> bool:
        """Validate Gordon Growth Model inputs"""
        errors = []

        if dividend <= 0:
            errors.append("Dividend must be positive for Gordon Growth Model")

        if growth_rate >= required_return:
            errors.append("Growth rate must be less than required return for Gordon Growth Model")

        if required_return <= 0:
            errors.append("Required return must be positive")

        if abs(required_return - growth_rate) < 0.01:
            errors.append("Required return and growth rate are too close - model becomes unstable")

        if errors:
            raise ValidationError("; ".join(errors))

        return True

    @staticmethod
    def validate_multistage_ddm_inputs(dividends: List[float], growth_rates: List[float],
                                       required_return: float, terminal_growth: float) -> bool:
        """Validate multi-stage DDM inputs"""
        errors = []

        if len(dividends) != len(growth_rates):
            errors.append("Number of dividends must match number of growth rates")

        if any(d <= 0 for d in dividends):
            errors.append("All dividends must be positive")

        if terminal_growth >= required_return:
            errors.append("Terminal growth rate must be less than required return")

        if terminal_growth > 0.06:  # Conservative long-term growth limit
            errors.append("Terminal growth rate should not exceed 6% for most companies")

        for i, gr in enumerate(growth_rates):
            if gr > 0.5:  # 50% growth rate warning
                errors.append(f"Growth rate of {gr:.2%} in period {i + 1} is very high")

        if errors:
            raise ValidationError("; ".join(errors))

        return True


class DCFValidator:
    """Validator for Discounted Cash Flow Models"""

    @staticmethod
    def validate_fcf_inputs(cash_flows: List[float], discount_rate: float,
                            terminal_value: Optional[float] = None) -> bool:
        """Validate Free Cash Flow inputs"""
        errors = []
        warnings = []

        if discount_rate <= 0:
            errors.append("Discount rate must be positive")

        if discount_rate > 0.25:
            warnings.append(f"Discount rate of {discount_rate:.2%} is very high")

        # Check for negative cash flows
        negative_cf_count = sum(1 for cf in cash_flows if cf < 0)
        if negative_cf_count > len(cash_flows) / 2:
            warnings.append("More than half of projected cash flows are negative")

        # Check for unrealistic growth in cash flows
        for i in range(1, len(cash_flows)):
            if cash_flows[i - 1] > 0 and cash_flows[i] > 0:
                growth = (cash_flows[i] / cash_flows[i - 1]) - 1
                if growth > 1.0:  # 100% growth
                    warnings.append(f"Cash flow growth of {growth:.2%} in year {i + 1} is very high")

        if terminal_value and terminal_value < 0:
            errors.append("Terminal value cannot be negative")

        if errors:
            raise ValidationError("; ".join(errors))

        if warnings:
            print("DCF Warnings:", "; ".join(warnings))

        return True

    @staticmethod
    def validate_fcff_calculation_inputs(ebit: float, tax_rate: float, depreciation: float,
                                         capex: float, working_capital_change: float) -> bool:
        """Validate FCFF calculation inputs"""
        errors = []

        if tax_rate < 0 or tax_rate > 1:
            errors.append("Tax rate must be between 0 and 1")

        if depreciation < 0:
            errors.append("Depreciation cannot be negative")

        if capex < 0:
            errors.append("Capital expenditures cannot be negative")

        # Warning for unusual values
        if tax_rate > 0.5:
            print(f"Warning: Tax rate of {tax_rate:.2%} is very high")

        if capex > abs(ebit) * 2:
            print("Warning: Capital expenditures are very high relative to EBIT")

        if errors:
            raise ValidationError("; ".join(errors))

        return True


class MultiplesValidator:
    """Validator for Market Multiple Valuation"""

    @staticmethod
    def validate_comparable_companies(comparables: List[Dict[str, Any]],
                                      target_company: Dict[str, Any]) -> bool:
        """Validate comparable companies selection"""
        errors = []
        warnings = []

        if len(comparables) < 3:
            warnings.append("Fewer than 3 comparable companies - results may be unreliable")

        # Check for similar business characteristics
        target_sector = target_company.get('sector', '')
        target_size = target_company.get('market_cap', 0)

        different_sector_count = 0
        size_differences = []

        for comp in comparables:
            # Sector comparison
            if comp.get('sector', '') != target_sector:
                different_sector_count += 1

            # Size comparison
            comp_size = comp.get('market_cap', 0)
            if target_size > 0 and comp_size > 0:
                size_ratio = max(comp_size, target_size) / min(comp_size, target_size)
                size_differences.append(size_ratio)

        if different_sector_count > len(comparables) / 2:
            warnings.append("More than half of comparables are from different sectors")

        if size_differences and max(size_differences) > 10:
            warnings.append("Some comparables differ significantly in size from target company")

        if warnings:
            print("Comparables Warnings:", "; ".join(warnings))

        return True

    @staticmethod
    def validate_multiple_values(multiples: Dict[str, float]) -> bool:
        """Validate individual multiple values"""
        errors = []
        warnings = []

        for metric, value in multiples.items():
            if value < 0:
                errors.append(f"{metric} cannot be negative")

            # Specific warnings for each multiple
            if metric == 'pe_ratio' and value > 50:
                warnings.append(f"P/E ratio of {value:.2f} is very high")
            elif metric == 'pb_ratio' and value > 5:
                warnings.append(f"P/B ratio of {value:.2f} is high")
            elif metric == 'ps_ratio' and value > 10:
                warnings.append(f"P/S ratio of {value:.2f} is high")
            elif metric == 'ev_ebitda' and value > 20:
                warnings.append(f"EV/EBITDA of {value:.2f} is high")

        if errors:
            raise ValidationError("; ".join(errors))

        if warnings:
            print("Multiple Validation Warnings:", "; ".join(warnings))

        return True


class ResidualIncomeValidator:
    """Validator for Residual Income Models"""

    @staticmethod
    def validate_ri_inputs(net_income: float, book_value: float,
                           required_return: float, roe: float) -> bool:
        """Validate Residual Income model inputs"""
        errors = []
        warnings = []

        if book_value <= 0:
            errors.append("Book value must be positive for Residual Income model")

        if required_return <= 0:
            errors.append("Required return must be positive")

        if required_return > 0.3:
            warnings.append(f"Required return of {required_return:.2%} is very high")

        # Check ROE vs required return relationship
        if roe > 0 and abs(roe - required_return) < 0.01:
            warnings.append("ROE and required return are very close - residual income will be minimal")

        # Check for consistency
        calculated_roe = net_income / book_value if book_value != 0 else 0
        if abs(calculated_roe - roe) > 0.02:  # 2% difference tolerance
            warnings.append("Provided ROE doesn't match calculated ROE from net income and book value")

        if errors:
            raise ValidationError("; ".join(errors))

        if warnings:
            print("Residual Income Warnings:", "; ".join(warnings))

        return True


class CompanyDataValidator:
    """Validator for Company Data Integrity"""

    @staticmethod
    def validate_company_data(company_data: CompanyData) -> Dict[str, List[str]]:
        """Comprehensive validation of company data"""
        errors = []
        warnings = []

        # Basic data validation
        if not company_data.symbol:
            errors.append("Company symbol is required")

        if company_data.current_price <= 0:
            errors.append("Current price must be positive")

        if company_data.shares_outstanding <= 0:
            errors.append("Shares outstanding must be positive")

        # Market cap consistency check
        calculated_market_cap = company_data.current_price * company_data.shares_outstanding
        if abs(calculated_market_cap - company_data.market_cap) / company_data.market_cap > 0.1:
            warnings.append("Market cap inconsistent with price × shares outstanding")

        # Financial data validation
        financial_data = company_data.financial_data

        # Revenue validation
        revenue = financial_data.get('revenue', 0)
        if revenue < 0:
            warnings.append("Negative revenue reported")

        # Profitability checks
        net_income = financial_data.get('net_income', 0)
        profit_margin = financial_data.get('profit_margin', 0)

        if revenue > 0 and net_income != 0:
            calculated_margin = net_income / revenue
            if abs(calculated_margin - profit_margin) > 0.02:
                warnings.append("Profit margin inconsistent with net income and revenue")

        # Balance sheet validation
        total_assets = financial_data.get('total_assets', 0)
        total_debt = financial_data.get('total_debt', 0)

        if total_debt > total_assets and total_assets > 0:
            warnings.append("Total debt exceeds total assets")

        # Ratio validation
        market_data = company_data.market_data
        pe_ratio = market_data.get('pe_ratio', 0)
        eps = financial_data.get('earnings_per_share', 0)

        if pe_ratio > 0 and eps > 0:
            calculated_price = pe_ratio * eps
            if abs(calculated_price - company_data.current_price) / company_data.current_price > 0.1:
                warnings.append("P/E ratio inconsistent with current price and EPS")

        return {'errors': errors, 'warnings': warnings}

    @staticmethod
    def validate_data_freshness(company_data: CompanyData, max_age_days: int = 7) -> bool:
        """Validate that data is recent enough for analysis"""
        if not company_data.last_updated:
            print("Warning: No timestamp available for data freshness check")
            return True

        age = datetime.now() - company_data.last_updated
        if age.days > max_age_days:
            print(f"Warning: Data is {age.days} days old (max recommended: {max_age_days} days)")

        return True


# Utility functions for quick validation
def validate_all_inputs(valuation_method: ValuationMethod, **kwargs) -> bool:
    """Master validation function for all models"""

    if valuation_method in [ValuationMethod.DDM_GORDON, ValuationMethod.DDM_TWO_STAGE,
                            ValuationMethod.DDM_THREE_STAGE, ValuationMethod.DDM_H_MODEL]:
        return DDMValidator.validate_gordon_growth_inputs(
            kwargs.get('dividend', 0),
            kwargs.get('growth_rate', 0),
            kwargs.get('required_return', 0)
        )

    elif valuation_method in [ValuationMethod.DCF_FCFF, ValuationMethod.DCF_FCFE]:
        return DCFValidator.validate_fcf_inputs(
            kwargs.get('cash_flows', []),
            kwargs.get('discount_rate', 0),
            kwargs.get('terminal_value')
        )

    elif valuation_method in [ValuationMethod.MULTIPLES_PE, ValuationMethod.MULTIPLES_PB,
                              ValuationMethod.MULTIPLES_PS, ValuationMethod.MULTIPLES_EV_EBITDA]:
        return MultiplesValidator.validate_multiple_values(
            kwargs.get('multiples', {})
        )

    elif valuation_method == ValuationMethod.RESIDUAL_INCOME:
        return ResidualIncomeValidator.validate_ri_inputs(
            kwargs.get('net_income', 0),
            kwargs.get('book_value', 0),
            kwargs.get('required_return', 0),
            kwargs.get('roe', 0)
        )

    return True


def comprehensive_data_validation(company_data: CompanyData) -> Dict[str, Any]:
    """Run all validation checks on company data"""

    results = {
        'data_integrity': CompanyDataValidator.validate_company_data(company_data),
        'financial_ratios': CFAValidator.validate_financial_ratios(company_data.market_data),
        'is_valid': True,
        'critical_errors': []
    }

    # Check for critical errors that would prevent analysis
    if results['data_integrity']['errors']:
        results['is_valid'] = False
        results['critical_errors'].extend(results['data_integrity']['errors'])

    if results['financial_ratios']['errors']:
        results['is_valid'] = False
        results['critical_errors'].extend(results['financial_ratios']['errors'])

    return results