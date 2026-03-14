"""SEC Data Adapter - Maps edgartools data to DCF model inputs"""
from typing import Dict, Any, Optional, List
import sys
from datetime import datetime, timedelta

try:
    from edgar import Company, get_filings
    EDGAR_AVAILABLE = True
except ImportError:
    EDGAR_AVAILABLE = False

try:
    import yfinance as yf
    YFINANCE_AVAILABLE = True
except ImportError:
    YFINANCE_AVAILABLE = False

try:
    from fredapi import Fred
    FRED_AVAILABLE = True
except ImportError:
    FRED_AVAILABLE = False


class SECDataAdapter:
    """Adapter to extract DCF inputs from SEC filings using edgartools"""

    def __init__(self, ticker: str, fred_api_key: Optional[str] = None):
        if not EDGAR_AVAILABLE:
            raise ImportError("edgartools not installed. Run: pip install edgartools")

        self.ticker = ticker.upper()
        self.company = None
        self.fred_api_key = fred_api_key

    def fetch_company_data(self) -> Dict[str, Any]:
        """Fetch company and latest 10-K filing"""
        try:
            self.company = Company(self.ticker)
            filings = self.company.get_filings(form="10-K").latest(1)

            if not filings:
                raise ValueError(f"No 10-K filings found for {self.ticker}")

            latest_10k = filings[0]

            return {
                'company_name': self.company.name,
                'cik': self.company.cik,
                'ticker': self.ticker,
                'filing': latest_10k,
                'filing_date': latest_10k.filing_date
            }
        except Exception as e:
            raise ValueError(f"Failed to fetch company data: {str(e)}")

    def extract_financials_from_10k(self, filing) -> Dict[str, Any]:
        """Extract financial data from 10-K filing"""
        try:
            # Get financial statements
            financials = filing.financials

            # Income statement
            income_stmt = financials.income_statement

            # Cash flow statement
            cash_flow = financials.cash_flow_statement

            # Balance sheet
            balance_sheet = financials.balance_sheet

            # Extract key metrics
            ebit = self._extract_ebit(income_stmt)
            depreciation = self._extract_depreciation(cash_flow)
            capex = self._extract_capex(cash_flow)
            nwc_change = self._extract_nwc_change(balance_sheet)
            cash = self._extract_cash(balance_sheet)
            total_debt = self._extract_total_debt(balance_sheet)
            shares_outstanding = self._extract_shares(balance_sheet)
            minority_interest = self._extract_minority_interest(balance_sheet)
            preferred_stock = self._extract_preferred_stock(balance_sheet)

            return {
                'ebit': ebit,
                'depreciation': depreciation,
                'capex': capex,
                'change_in_nwc': nwc_change,
                'cash': cash,
                'debt': total_debt,
                'shares_outstanding': shares_outstanding,
                'minority_interest': minority_interest,
                'preferred_stock': preferred_stock
            }
        except Exception as e:
            raise ValueError(f"Failed to extract financials: {str(e)}")

    def _extract_ebit(self, income_stmt) -> float:
        """Extract EBIT from income statement"""
        # Try multiple GAAP tags
        tags = [
            'OperatingIncomeLoss',
            'IncomeLossFromContinuingOperationsBeforeIncomeTaxesExtraordinaryItemsNoncontrollingInterest',
            'EBIT'
        ]

        for tag in tags:
            value = self._get_value_from_stmt(income_stmt, tag)
            if value is not None:
                return float(value)

        # Fallback: Calculate from Revenue - COGS - OpEx
        try:
            revenue = self._get_value_from_stmt(income_stmt, 'Revenues')
            cogs = self._get_value_from_stmt(income_stmt, 'CostOfRevenue')
            opex = self._get_value_from_stmt(income_stmt, 'OperatingExpenses')

            if all(v is not None for v in [revenue, cogs, opex]):
                return float(revenue - cogs - opex)
        except:
            pass

        raise ValueError("Could not extract EBIT from income statement")

    def _extract_depreciation(self, cash_flow) -> float:
        """Extract depreciation from cash flow statement"""
        tags = [
            'DepreciationDepletionAndAmortization',
            'Depreciation',
            'DepreciationAndAmortization'
        ]

        for tag in tags:
            value = self._get_value_from_stmt(cash_flow, tag)
            if value is not None:
                return float(value)

        return 0.0

    def _extract_capex(self, cash_flow) -> float:
        """Extract CapEx from cash flow statement"""
        tags = [
            'PaymentsToAcquirePropertyPlantAndEquipment',
            'CapitalExpenditures',
            'PaymentsForCapitalImprovements'
        ]

        for tag in tags:
            value = self._get_value_from_stmt(cash_flow, tag)
            if value is not None:
                return abs(float(value))  # CapEx is usually negative in cash flow

        return 0.0

    def _extract_nwc_change(self, balance_sheet) -> float:
        """Calculate change in net working capital"""
        try:
            # Get current and prior year balance sheets
            current_ca = self._get_value_from_stmt(balance_sheet, 'AssetsCurrent')
            current_cl = self._get_value_from_stmt(balance_sheet, 'LiabilitiesCurrent')

            if current_ca is not None and current_cl is not None:
                current_nwc = float(current_ca - current_cl)

                # Try to get prior year (simplified - assumes 0 change if not available)
                return 0.0  # TODO: Implement prior year comparison

            return 0.0
        except:
            return 0.0

    def _extract_cash(self, balance_sheet) -> float:
        """Extract cash and cash equivalents"""
        tags = [
            'CashAndCashEquivalentsAtCarryingValue',
            'Cash',
            'CashCashEquivalentsAndShortTermInvestments'
        ]

        for tag in tags:
            value = self._get_value_from_stmt(balance_sheet, tag)
            if value is not None:
                return float(value)

        return 0.0

    def _extract_total_debt(self, balance_sheet) -> float:
        """Extract total debt"""
        tags = [
            'DebtCurrent',
            'LongTermDebt',
            'DebtLongTerm'
        ]

        total_debt = 0.0
        for tag in tags:
            value = self._get_value_from_stmt(balance_sheet, tag)
            if value is not None:
                total_debt += float(value)

        return total_debt

    def _extract_shares(self, balance_sheet) -> float:
        """Extract shares outstanding"""
        tags = [
            'CommonStockSharesOutstanding',
            'SharesOutstanding'
        ]

        for tag in tags:
            value = self._get_value_from_stmt(balance_sheet, tag)
            if value is not None:
                return float(value)

        raise ValueError("Could not extract shares outstanding")

    def _extract_minority_interest(self, balance_sheet) -> float:
        """Extract minority interest"""
        tags = ['MinorityInterest', 'NoncontrollingInterestInConsolidatedEntity']

        for tag in tags:
            value = self._get_value_from_stmt(balance_sheet, tag)
            if value is not None:
                return float(value)

        return 0.0

    def _extract_preferred_stock(self, balance_sheet) -> float:
        """Extract preferred stock"""
        tags = ['PreferredStockValue', 'PreferredStock']

        for tag in tags:
            value = self._get_value_from_stmt(balance_sheet, tag)
            if value is not None:
                return float(value)

        return 0.0

    def _get_value_from_stmt(self, statement, tag: str) -> Optional[float]:
        """Helper to extract value from financial statement"""
        try:
            if hasattr(statement, tag):
                value = getattr(statement, tag)
                if value is not None:
                    return float(value)

            # Try as dict key
            if hasattr(statement, 'get') and callable(statement.get):
                value = statement.get(tag)
                if value is not None:
                    return float(value)

            return None
        except:
            return None

    def calculate_beta(self, period: str = "2y") -> float:
        """Calculate beta using market data (yfinance)"""
        if not YFINANCE_AVAILABLE:
            return 1.0  # Default beta if yfinance not available

        try:
            stock = yf.Ticker(self.ticker)
            info = stock.info

            # Try to get beta from info
            if 'beta' in info and info['beta'] is not None:
                beta = float(info['beta'])
                # Bounds check
                if 0.1 <= beta <= 3.0:
                    return beta

            # Fallback: Calculate manually
            stock_data = stock.history(period=period)
            market_data = yf.Ticker("^GSPC").history(period=period)  # S&P 500

            if len(stock_data) > 0 and len(market_data) > 0:
                stock_returns = stock_data['Close'].pct_change().dropna()
                market_returns = market_data['Close'].pct_change().dropna()

                # Align dates
                aligned = stock_returns.align(market_returns, join='inner')

                if len(aligned[0]) > 20:
                    covariance = aligned[0].cov(aligned[1])
                    market_variance = aligned[1].var()

                    if market_variance > 0:
                        beta = covariance / market_variance
                        # Bounds check
                        return max(0.1, min(beta, 3.0))

            return 1.0  # Default market beta
        except Exception as e:
            print(f"Beta calculation failed: {e}, using default 1.0")
            return 1.0

    def get_risk_free_rate(self, default: float = 0.045) -> float:
        """Get current risk-free rate from FRED (10-year Treasury)

        Args:
            default: Default rate if API fails (default: 4.5%)
        """
        if not FRED_AVAILABLE or not self.fred_api_key:
            return default

        try:
            fred = Fred(api_key=self.fred_api_key)
            # 10-year Treasury rate (DGS10)
            rate_series = fred.get_series('DGS10', observation_start=datetime.now() - timedelta(days=30))

            if len(rate_series) > 0:
                latest_rate = rate_series.iloc[-1]
                return float(latest_rate) / 100.0  # Convert from percentage

            return default
        except Exception as e:
            print(f"FRED API failed: {e}, using default {default:.2%}")
            return default

    def get_market_risk_premium(self, default: float = 0.065) -> float:
        """Get market risk premium (historical average)

        Args:
            default: Default premium (default: 6.5%)
        """
        return default

    def estimate_cost_of_debt(self, total_debt: float, filing, default: float = 0.05) -> float:
        """Estimate cost of debt from 10-K debt footnotes

        Args:
            total_debt: Total debt amount
            filing: SEC filing object
            default: Default cost if calculation fails (default: 5%)
        """
        try:
            # Try to extract interest expense from income statement
            financials = filing.financials
            income_stmt = financials.income_statement

            interest_expense = self._get_value_from_stmt(income_stmt, 'InterestExpense')

            if interest_expense and total_debt > 0:
                cost_of_debt = abs(float(interest_expense)) / float(total_debt)
                # Bounds check (0.5% to 15%)
                return max(0.005, min(cost_of_debt, 0.15))

            return default
        except:
            return default

    def estimate_tax_rate(self, filing, default: float = 0.21) -> float:
        """Extract effective tax rate from 10-K

        Args:
            filing: SEC filing object
            default: Default tax rate if extraction fails (default: 21% US statutory)
        """
        try:
            financials = filing.financials
            income_stmt = financials.income_statement

            # Get income tax expense and pretax income
            tax_expense = self._get_value_from_stmt(income_stmt, 'IncomeTaxExpenseBenefit')
            pretax_income = self._get_value_from_stmt(income_stmt, 'IncomeLossFromContinuingOperationsBeforeIncomeTaxesExtraordinaryItemsNoncontrollingInterest')

            if tax_expense and pretax_income and pretax_income > 0:
                tax_rate = float(tax_expense) / float(pretax_income)
                # Bounds check (0% to 50%)
                return max(0.0, min(tax_rate, 0.50))

            return default
        except:
            return default

    def estimate_growth_rates(self, years: int = 5, default_growth: float = 0.05,
                             taper: bool = True) -> List[float]:
        """Estimate revenue growth rates

        Args:
            years: Number of years to project
            default_growth: Default growth rate if estimation fails (default: 5%)
            taper: Whether to taper growth over time (default: True)
        """
        try:
            if not YFINANCE_AVAILABLE:
                return [default_growth] * years

            stock = yf.Ticker(self.ticker)

            # Try to get analyst growth estimates
            if hasattr(stock, 'info') and 'revenueGrowth' in stock.info:
                base_growth = float(stock.info['revenueGrowth'])

                if taper:
                    # Taper growth over projection period
                    growth_rates = []
                    for year in range(years):
                        # Decay growth by 10% each year
                        growth = base_growth * (0.9 ** year)
                        growth_rates.append(max(0.02, min(growth, 0.30)))  # 2% to 30% bounds
                    return growth_rates
                else:
                    # Constant growth
                    return [max(0.02, min(base_growth, 0.30))] * years

            return [default_growth] * years
        except:
            return [default_growth] * years

    def get_market_cap(self) -> float:
        """Get current market capitalization"""
        if not YFINANCE_AVAILABLE:
            return 0.0

        try:
            stock = yf.Ticker(self.ticker)
            info = stock.info

            if 'marketCap' in info and info['marketCap']:
                return float(info['marketCap'])

            return 0.0
        except:
            return 0.0

    def build_dcf_inputs(self, fred_api_key: Optional[str] = None,
                        overrides: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """Build complete DCF model inputs from SEC filings and market data

        Args:
            fred_api_key: FRED API key for risk-free rate (optional)
            overrides: Dict of user-specified values to override defaults/calculations

        Override options:
            - beta: Custom beta value
            - risk_free_rate: Custom risk-free rate
            - market_risk_premium: Custom market risk premium
            - cost_of_debt: Custom cost of debt
            - tax_rate: Custom tax rate
            - growth_rates: List of custom growth rates
            - terminal_growth_rate: Custom terminal growth
            - default_growth: Default growth for estimation
            - taper_growth: Whether to taper growth (True/False)
        """
        overrides = overrides or {}

        # Fetch company and filing data
        company_data = self.fetch_company_data()
        filing = company_data['filing']

        # Extract financials from 10-K
        financials = self.extract_financials_from_10k(filing)

        # Get market data with overrides
        beta = overrides.get('beta') or self.calculate_beta()
        risk_free_rate = overrides.get('risk_free_rate') or self.get_risk_free_rate(
            default=overrides.get('default_risk_free_rate', 0.045)
        )
        market_risk_premium = overrides.get('market_risk_premium') or self.get_market_risk_premium(
            default=overrides.get('default_market_risk_premium', 0.065)
        )
        cost_of_debt = overrides.get('cost_of_debt') or self.estimate_cost_of_debt(
            financials['debt'], filing, default=overrides.get('default_cost_of_debt', 0.05)
        )
        tax_rate = overrides.get('tax_rate') or self.estimate_tax_rate(
            filing, default=overrides.get('default_tax_rate', 0.21)
        )
        market_cap = overrides.get('market_value_equity') or self.get_market_cap()

        # Growth rates with overrides
        if 'growth_rates' in overrides:
            growth_rates = overrides['growth_rates']
        else:
            growth_rates = self.estimate_growth_rates(
                years=overrides.get('projection_years', 5),
                default_growth=overrides.get('default_growth', 0.05),
                taper=overrides.get('taper_growth', True)
            )

        # Build WACC inputs
        wacc_inputs = {
            'risk_free_rate': risk_free_rate,
            'market_risk_premium': market_risk_premium,
            'beta': beta,
            'cost_of_debt': cost_of_debt,
            'tax_rate': tax_rate,
            'market_value_equity': market_cap,
            'market_value_debt': overrides.get('market_value_debt') or financials['debt']
        }

        # Build FCF inputs - allow overrides
        fcf_inputs = {
            'ebit': overrides.get('ebit') or financials['ebit'],
            'tax_rate': tax_rate,
            'depreciation': overrides.get('depreciation') or financials['depreciation'],
            'capex': overrides.get('capex') or financials['capex'],
            'change_in_nwc': overrides.get('change_in_nwc') or financials['change_in_nwc']
        }

        # Build balance sheet inputs - allow overrides
        balance_sheet = {
            'cash': overrides.get('cash') or financials['cash'],
            'debt': overrides.get('debt') or financials['debt'],
            'minority_interest': overrides.get('minority_interest') or financials['minority_interest'],
            'preferred_stock': overrides.get('preferred_stock') or financials['preferred_stock'],
            'diluted_shares': overrides.get('diluted_shares') or financials['shares_outstanding']
        }

        return {
            'company_name': company_data['company_name'],
            'ticker': self.ticker,
            'filing_date': str(company_data['filing_date']),
            'wacc_inputs': wacc_inputs,
            'fcf_inputs': fcf_inputs,
            'growth_rates': growth_rates,
            'terminal_growth_rate': overrides.get('terminal_growth_rate', 0.025),
            'balance_sheet': balance_sheet,
            'shares_outstanding': overrides.get('shares_outstanding') or financials['shares_outstanding'],
            'data_sources': {
                'sec_filing': f"10-K filed {company_data['filing_date']}",
                'beta_source': 'user_override' if 'beta' in overrides else ('yfinance' if YFINANCE_AVAILABLE else 'default'),
                'risk_free_rate_source': 'user_override' if 'risk_free_rate' in overrides else ('FRED' if (FRED_AVAILABLE and self.fred_api_key) else 'default'),
                'overrides_applied': list(overrides.keys()) if overrides else []
            }
        }


def main():
    """CLI entry point

    Usage:
        sec_data_adapter.py <ticker> [fred_api_key] [overrides_json]

    Examples:
        # Basic usage
        python sec_data_adapter.py AAPL

        # With FRED API key
        python sec_data_adapter.py AAPL your_fred_key

        # With custom overrides
        python sec_data_adapter.py AAPL "" '{"beta": 1.5, "terminal_growth_rate": 0.03}'
    """
    import json

    if len(sys.argv) < 2:
        result = {
            "success": False,
            "error": "Usage: sec_data_adapter.py <ticker> [fred_api_key] [overrides_json]"
        }
        print(json.dumps(result))
        sys.exit(1)

    ticker = sys.argv[1]
    fred_api_key = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] else None
    overrides = json.loads(sys.argv[3]) if len(sys.argv) > 3 else None

    try:
        adapter = SECDataAdapter(ticker, fred_api_key)
        dcf_inputs = adapter.build_dcf_inputs(fred_api_key, overrides)

        result = {
            "success": True,
            "data": dcf_inputs
        }
        print(json.dumps(result, default=str))

    except Exception as e:
        result = {
            "success": False,
            "error": str(e),
            "ticker": ticker
        }
        print(json.dumps(result))
        sys.exit(1)


if __name__ == '__main__':
    main()
