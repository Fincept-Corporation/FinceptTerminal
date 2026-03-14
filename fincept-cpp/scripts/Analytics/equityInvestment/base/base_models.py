"""
Equity Investment Base Models Module
====================================

Abstract base classes and common interfaces for equity investment analytics.
Provides foundational models for equity valuation, fundamental analysis, and
research methodologies compliant with CFA Institute standards for equity
analysis and portfolio management across all market segments and asset classes.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Financial statements (income statement, balance sheet, cash flow)
  - Market data and price series with historical performance
  - Company fundamentals and operational metrics
  - Industry and sector benchmark data
  - Economic indicators and market conditions

OUTPUT:
  - Standardized model interfaces and abstract classes
  - Equity valuation model frameworks
  - Fundamental analysis template structures
  - Research methodology guidelines
  - Portfolio analytics base implementations

PARAMETERS:
  - valuation_method: Primary valuation approach (default: 'DCF')
  - discount_rate_method: Discount rate calculation method (default: 'WACC')
  - growth_assumption: Long-term growth rate assumption (default: 0.025)
  - terminal_multiple: Terminal value multiple (default: 15.0)
  - currency_reporting: Reporting currency (default: 'USD')
  - fiscal_year_end: Company fiscal year end (default: '12-31')
"""

from abc import ABC, abstractmethod
from typing import Dict, Any, Optional, List, Union
from dataclasses import dataclass
from enum import Enum
import pandas as pd
import numpy as np
from datetime import datetime


class ValuationMethod(Enum):
    """Enumeration of available valuation methods"""
    DDM_GORDON = "dividend_discount_gordon"
    DDM_TWO_STAGE = "dividend_discount_two_stage"
    DDM_THREE_STAGE = "dividend_discount_three_stage"
    DDM_H_MODEL = "dividend_discount_h_model"
    DCF_FCFF = "dcf_free_cash_flow_firm"
    DCF_FCFE = "dcf_free_cash_flow_equity"
    MULTIPLES_PE = "price_earnings_multiple"
    MULTIPLES_PB = "price_book_multiple"
    MULTIPLES_PS = "price_sales_multiple"
    MULTIPLES_EV_EBITDA = "enterprise_value_ebitda"
    RESIDUAL_INCOME = "residual_income_model"
    PRIVATE_INCOME = "private_income_approach"
    PRIVATE_MARKET = "private_market_approach"
    PRIVATE_ASSET = "private_asset_approach"


class MarketEfficiencyForm(Enum):
    """Market efficiency forms as per CFA curriculum"""
    WEAK = "weak_form"
    SEMI_STRONG = "semi_strong_form"
    STRONG = "strong_form"


class SecurityType(Enum):
    """Types of securities for analysis"""
    COMMON_STOCK = "common_stock"
    PREFERRED_STOCK = "preferred_stock"
    CORPORATE_BOND = "corporate_bond"
    GOVERNMENT_BOND = "government_bond"
    OPTION = "option"
    FUTURE = "future"
    COMMODITY = "commodity"
    CURRENCY = "currency"


@dataclass
class CompanyData:
    """Standard company data structure"""
    symbol: str
    name: str
    sector: str
    industry: str
    market_cap: float
    shares_outstanding: float
    current_price: float
    financial_data: Dict[str, Any]
    market_data: Dict[str, Any]
    last_updated: datetime


@dataclass
class ValuationResult:
    """Standard valuation result structure"""
    method: ValuationMethod
    intrinsic_value: float
    current_price: float
    recommendation: str  # "BUY", "HOLD", "SELL"
    upside_downside: float
    confidence_level: str  # "HIGH", "MEDIUM", "LOW"
    assumptions: Dict[str, Any]
    sensitivity_analysis: Optional[Dict[str, float]] = None
    calculation_details: Optional[Dict[str, Any]] = None


@dataclass
class MarketData:
    """Market data structure"""
    risk_free_rate: float
    market_return: float
    beta: float
    dividend_yield: float
    growth_rate: float
    required_return: float


class BaseAnalyticalModel(ABC):
    """Abstract base class for all analytical models"""

    def __init__(self, name: str, description: str):
        self.name = name
        self.description = description
        self.last_calculation = None
        self.assumptions = {}

    @abstractmethod
    def validate_inputs(self, **kwargs) -> bool:
        """Validate input parameters for the model"""
        pass

    @abstractmethod
    def calculate(self, **kwargs) -> ValuationResult:
        """Perform the main calculation"""
        pass

    def get_assumptions(self) -> Dict[str, Any]:
        """Return current model assumptions"""
        return self.assumptions.copy()

    def set_assumptions(self, **kwargs):
        """Set model assumptions"""
        self.assumptions.update(kwargs)


class BaseValuationModel(BaseAnalyticalModel):
    """Base class for equity valuation models"""

    def __init__(self, name: str, description: str):
        super().__init__(name, description)
        self.valuation_method = None

    @abstractmethod
    def calculate_intrinsic_value(self, company_data: CompanyData, market_data: MarketData) -> float:
        """Calculate intrinsic value of the security"""
        pass

    def generate_recommendation(self, intrinsic_value: float, current_price: float) -> str:
        """Generate buy/hold/sell recommendation"""
        upside = (intrinsic_value - current_price) / current_price

        if upside > 0.15:
            return "BUY"
        elif upside < -0.15:
            return "SELL"
        else:
            return "HOLD"

    def calculate_upside_downside(self, intrinsic_value: float, current_price: float) -> float:
        """Calculate percentage upside/downside"""
        return (intrinsic_value - current_price) / current_price


class BaseMarketAnalysisModel(BaseAnalyticalModel):
    """Base class for market analysis models"""

    def __init__(self, name: str, description: str):
        super().__init__(name, description)

    @abstractmethod
    def analyze_market_data(self, market_data: pd.DataFrame) -> Dict[str, Any]:
        """Analyze market data and return insights"""
        pass


class BaseCompanyAnalysisModel(BaseAnalyticalModel):
    """Base class for company analysis models"""

    def __init__(self, name: str, description: str):
        super().__init__(name, description)

    @abstractmethod
    def analyze_company(self, company_data: CompanyData) -> Dict[str, Any]:
        """Analyze company fundamentals"""
        pass


class DataProvider(ABC):
    """Abstract base class for data providers"""

    @abstractmethod
    def get_company_data(self, symbol: str) -> CompanyData:
        """Retrieve company data"""
        pass

    @abstractmethod
    def get_market_data(self, symbol: str) -> MarketData:
        """Retrieve market data"""
        pass

    @abstractmethod
    def get_financial_statements(self, symbol: str, period: str = "annual") -> Dict[str, pd.DataFrame]:
        """Retrieve financial statements"""
        pass

    @abstractmethod
    def get_price_data(self, symbol: str, start_date: str, end_date: str) -> pd.DataFrame:
        """Retrieve historical price data"""
        pass


class CalculationEngine:
    """Core calculation engine with common financial formulas"""

    @staticmethod
    def present_value(future_value: float, rate: float, periods: int) -> float:
        """Calculate present value"""
        return future_value / ((1 + rate) ** periods)

    @staticmethod
    def future_value(present_value: float, rate: float, periods: int) -> float:
        """Calculate future value"""
        return present_value * ((1 + rate) ** periods)

    @staticmethod
    def npv(cash_flows: List[float], discount_rate: float) -> float:
        """Calculate Net Present Value"""
        npv = 0
        for i, cf in enumerate(cash_flows):
            npv += cf / ((1 + discount_rate) ** i)
        return npv

    @staticmethod
    def irr(cash_flows: List[float], guess: float = 0.1) -> float:
        """Calculate Internal Rate of Return using Newton-Raphson method"""
        rate = guess
        tolerance = 1e-6
        max_iterations = 100

        for _ in range(max_iterations):
            npv = sum(cf / ((1 + rate) ** i) for i, cf in enumerate(cash_flows))
            npv_derivative = sum(-i * cf / ((1 + rate) ** (i + 1)) for i, cf in enumerate(cash_flows))

            if abs(npv) < tolerance:
                return rate

            rate = rate - npv / npv_derivative

        return rate

    @staticmethod
    def capm_required_return(risk_free_rate: float, beta: float, market_return: float) -> float:
        """Calculate required return using CAPM"""
        return risk_free_rate + beta * (market_return - risk_free_rate)

    @staticmethod
    def gordon_growth_model(dividend: float, growth_rate: float, required_return: float) -> float:
        """Calculate value using Gordon Growth Model"""
        if required_return <= growth_rate:
            raise ValueError("Required return must be greater than growth rate")
        return dividend * (1 + growth_rate) / (required_return - growth_rate)

    @staticmethod
    def sustainable_growth_rate(roe: float, payout_ratio: float) -> float:
        """Calculate sustainable growth rate"""
        retention_ratio = 1 - payout_ratio
        return roe * retention_ratio

    @staticmethod
    def dupont_roe(net_margin: float, asset_turnover: float, equity_multiplier: float) -> float:
        """Calculate ROE using DuPont analysis"""
        return net_margin * asset_turnover * equity_multiplier

    @staticmethod
    def pe_ratio_from_fundamentals(payout_ratio: float, required_return: float, growth_rate: float) -> float:
        """Calculate justified P/E ratio from fundamentals"""
        if required_return <= growth_rate:
            raise ValueError("Required return must be greater than growth rate")
        return payout_ratio * (1 + growth_rate) / (required_return - growth_rate)

    @staticmethod
    def free_cash_flow_to_equity(net_income: float, depreciation: float, capex: float,
                                 working_capital_change: float, net_borrowing: float) -> float:
        """Calculate Free Cash Flow to Equity"""
        return net_income + depreciation - capex - working_capital_change + net_borrowing

    @staticmethod
    def free_cash_flow_to_firm(ebit: float, tax_rate: float, depreciation: float,
                               capex: float, working_capital_change: float) -> float:
        """Calculate Free Cash Flow to Firm"""
        nopat = ebit * (1 - tax_rate)
        return nopat + depreciation - capex - working_capital_change

    @staticmethod
    def residual_income(net_income: float, beginning_book_value: float, required_return: float) -> float:
        """Calculate Residual Income"""
        equity_charge = beginning_book_value * required_return
        return net_income - equity_charge

    @staticmethod
    def economic_value_added(nopat: float, invested_capital: float, wacc: float) -> float:
        """Calculate Economic Value Added (EVA)"""
        capital_charge = invested_capital * wacc
        return nopat - capital_charge


class ModelValidator:
    """Validation utilities for model inputs"""

    @staticmethod
    def validate_positive_number(value: Union[int, float], name: str) -> bool:
        """Validate that a number is positive"""
        if not isinstance(value, (int, float)) or value <= 0:
            raise ValueError(f"{name} must be a positive number")
        return True

    @staticmethod
    def validate_percentage(value: float, name: str, allow_negative: bool = False) -> bool:
        """Validate percentage values"""
        if not isinstance(value, (int, float)):
            raise ValueError(f"{name} must be a number")
        if not allow_negative and value < 0:
            raise ValueError(f"{name} cannot be negative")
        if value > 1:
            raise ValueError(f"{name} appears to be in percentage form, please use decimal (e.g., 0.05 for 5%)")
        return True

    @staticmethod
    def validate_growth_vs_required_return(growth_rate: float, required_return: float) -> bool:
        """Validate that growth rate is less than required return for DDM models"""
        if growth_rate >= required_return:
            raise ValueError("Growth rate must be less than required return for stable growth models")
        return True

    @staticmethod
    def validate_company_data(company_data: CompanyData) -> bool:
        """Validate company data structure"""
        required_fields = ['symbol', 'current_price', 'shares_outstanding']
        for field in required_fields:
            if not hasattr(company_data, field) or getattr(company_data, field) is None:
                raise ValueError(f"Company data missing required field: {field}")
        return True


# Exception classes for better error handling
class FinceptAnalyticsError(Exception):
    """Base exception for analytics module"""
    pass


class DataProviderError(FinceptAnalyticsError):
    """Exception for data provider issues"""
    pass


class ValidationError(FinceptAnalyticsError):
    """Exception for input validation failures"""
    pass


class CalculationError(FinceptAnalyticsError):
    """Exception for calculation failures"""
    pass


class ModelError(FinceptAnalyticsError):
    """Exception for model-specific issues"""
    pass