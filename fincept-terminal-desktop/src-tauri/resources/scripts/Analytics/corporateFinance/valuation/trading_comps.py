"""Trading Comparables Analysis - Public Company Comps"""
import sys
from pathlib import Path
from typing import Dict, Any, List, Optional
import numpy as np
from dataclasses import dataclass
from statistics import median, mean, stdev
from datetime import datetime

scripts_path = Path(__file__).parent.parent.parent.parent
sys.path.append(str(scripts_path))

try:
    import yfinance as yf
except ImportError:
    yf = None

@dataclass
class TradingComp:
    ticker: str
    company_name: str
    market_cap: float
    enterprise_value: float
    revenue_ltm: float
    ebitda_ltm: float
    ebit_ltm: float
    net_income_ltm: float
    total_debt: float
    cash: float
    shares_outstanding: float
    stock_price: float
    ev_revenue: float
    ev_ebitda: float
    ev_ebit: float
    price_earnings: float
    price_book: float
    price_sales: float
    revenue_growth: float
    ebitda_margin: float
    net_margin: float
    roe: float
    roa: float

class TradingCompsAnalyzer:
    def __init__(self):
        if yf is None:
            raise ImportError("yfinance required: pip install yfinance")

        self.industry_tickers = {
            'Technology': ['AAPL', 'MSFT', 'GOOGL', 'AMZN', 'META', 'NVDA', 'ADBE', 'CRM', 'ORCL', 'IBM'],
            'Healthcare': ['UNH', 'JNJ', 'PFE', 'ABBV', 'TMO', 'ABT', 'DHR', 'BMY', 'LLY', 'AMGN'],
            'Financial Services': ['JPM', 'BAC', 'WFC', 'C', 'GS', 'MS', 'BLK', 'SCHW', 'USB', 'PNC'],
            'Industrials': ['BA', 'HON', 'UNP', 'CAT', 'GE', 'MMM', 'LMT', 'RTX', 'DE', 'EMR'],
            'Consumer Discretionary': ['AMZN', 'TSLA', 'HD', 'NKE', 'MCD', 'LOW', 'SBUX', 'TGT', 'TJX', 'F'],
            'Consumer Staples': ['WMT', 'PG', 'KO', 'PEP', 'COST', 'PM', 'MO', 'CL', 'MDLZ', 'KMB'],
            'Energy': ['XOM', 'CVX', 'COP', 'SLB', 'EOG', 'PXD', 'MPC', 'PSX', 'VLO', 'OXY'],
            'Materials': ['LIN', 'APD', 'SHW', 'ECL', 'DD', 'NEM', 'FCX', 'NUE', 'VMC', 'MLM']
        }

    def fetch_comp_data(self, ticker: str) -> Optional[TradingComp]:
        """Fetch comprehensive financial data for single company"""
        try:
            stock = yf.Ticker(ticker)
            info = stock.info
            financials = stock.financials
            balance_sheet = stock.balance_sheet

            market_cap = info.get('marketCap', 0)
            shares_outstanding = info.get('sharesOutstanding', 0)
            stock_price = info.get('currentPrice', info.get('regularMarketPrice', 0))
            total_debt = info.get('totalDebt', 0)
            cash = info.get('totalCash', 0)
            enterprise_value = market_cap + total_debt - cash

            revenue_ltm = info.get('totalRevenue', 0)
            ebitda_ltm = info.get('ebitda', 0)
            # yfinance: 'ebit' is often None, derive from operatingMargins
            ebit_ltm = info.get('ebit', 0) or 0
            if ebit_ltm == 0 and revenue_ltm and info.get('operatingMargins'):
                ebit_ltm = revenue_ltm * info['operatingMargins']
            if ebit_ltm == 0:
                ebit_ltm = ebitda_ltm
            # yfinance: 'netIncome' is often None, use 'netIncomeToCommon' or profitMargins
            net_income_ltm = info.get('netIncome', 0) or info.get('netIncomeToCommon', 0) or 0
            if net_income_ltm == 0 and revenue_ltm and info.get('profitMargins'):
                net_income_ltm = revenue_ltm * info['profitMargins']

            revenue_growth = info.get('revenueGrowth', 0) * 100 if info.get('revenueGrowth') else 0
            ebitda_margin = (ebitda_ltm / revenue_ltm * 100) if revenue_ltm else 0
            net_margin = (net_income_ltm / revenue_ltm * 100) if revenue_ltm and net_income_ltm else 0
            roe = info.get('returnOnEquity', 0) * 100 if info.get('returnOnEquity') else 0
            roa = info.get('returnOnAssets', 0) * 100 if info.get('returnOnAssets') else 0

            ev_revenue = enterprise_value / revenue_ltm if revenue_ltm else 0
            ev_ebitda = enterprise_value / ebitda_ltm if ebitda_ltm else 0
            ev_ebit = enterprise_value / ebit_ltm if ebit_ltm else 0
            price_earnings = info.get('trailingPE', 0)
            price_book = info.get('priceToBook', 0)
            price_sales = info.get('priceToSalesTrailing12Months', 0)

            return TradingComp(
                ticker=ticker,
                company_name=info.get('longName', ticker),
                market_cap=market_cap,
                enterprise_value=enterprise_value,
                revenue_ltm=revenue_ltm,
                ebitda_ltm=ebitda_ltm,
                ebit_ltm=ebit_ltm,
                net_income_ltm=net_income_ltm,
                total_debt=total_debt,
                cash=cash,
                shares_outstanding=shares_outstanding,
                stock_price=stock_price,
                ev_revenue=ev_revenue,
                ev_ebitda=ev_ebitda,
                ev_ebit=ev_ebit,
                price_earnings=price_earnings,
                price_book=price_book,
                price_sales=price_sales,
                revenue_growth=revenue_growth,
                ebitda_margin=ebitda_margin,
                net_margin=net_margin,
                roe=roe,
                roa=roa
            )

        except Exception as e:
            print(f"Error fetching {ticker}: {e}")
            return None

    def find_comparables(self, industry: str, custom_tickers: Optional[List[str]] = None) -> List[TradingComp]:
        """Find public company comparables"""

        tickers = custom_tickers or self.industry_tickers.get(industry, [])

        comps = []
        for ticker in tickers:
            comp = self.fetch_comp_data(ticker)
            if comp and comp.market_cap > 0:
                comps.append(comp)

        return comps

    def calculate_statistics(self, comps: List[TradingComp]) -> Dict[str, Dict[str, float]]:
        """Calculate trading multiples statistics"""

        def calc_stats(values: List[float], name: str) -> Dict[str, float]:
            clean_vals = [v for v in values if v > 0 and not np.isnan(v) and not np.isinf(v)]
            if not clean_vals:
                return {f'{name}_mean': 0, f'{name}_median': 0, f'{name}_min': 0,
                       f'{name}_max': 0, f'{name}_std': 0, f'{name}_count': 0}

            return {
                f'{name}_mean': mean(clean_vals),
                f'{name}_median': median(clean_vals),
                f'{name}_min': min(clean_vals),
                f'{name}_max': max(clean_vals),
                f'{name}_std': stdev(clean_vals) if len(clean_vals) > 1 else 0,
                f'{name}_count': len(clean_vals),
                f'{name}_q1': np.percentile(clean_vals, 25),
                f'{name}_q3': np.percentile(clean_vals, 75)
            }

        stats = {}

        stats['ev_revenue'] = calc_stats([c.ev_revenue for c in comps], 'ev_revenue')
        stats['ev_ebitda'] = calc_stats([c.ev_ebitda for c in comps], 'ev_ebitda')
        stats['ev_ebit'] = calc_stats([c.ev_ebit for c in comps], 'ev_ebit')
        stats['price_earnings'] = calc_stats([c.price_earnings for c in comps], 'pe')
        stats['price_book'] = calc_stats([c.price_book for c in comps], 'pb')
        stats['price_sales'] = calc_stats([c.price_sales for c in comps], 'ps')
        stats['revenue_growth'] = calc_stats([c.revenue_growth for c in comps], 'rev_growth')
        stats['ebitda_margin'] = calc_stats([c.ebitda_margin for c in comps], 'ebitda_margin')
        stats['net_margin'] = calc_stats([c.net_margin for c in comps], 'net_margin')
        stats['roe'] = calc_stats([c.roe for c in comps], 'roe')

        return stats

    def apply_trading_multiples(self, target_financials: Dict[str, float],
                                comps: List[TradingComp]) -> Dict[str, Any]:
        """Apply trading multiples to target"""

        stats = self.calculate_statistics(comps)
        valuations = {}

        if target_financials.get('revenue'):
            revenue = target_financials['revenue']
            valuations['ev_revenue_median'] = revenue * stats['ev_revenue']['ev_revenue_median']
            valuations['ev_revenue_mean'] = revenue * stats['ev_revenue']['ev_revenue_mean']
            valuations['ev_revenue_q1'] = revenue * stats['ev_revenue']['ev_revenue_q1']
            valuations['ev_revenue_q3'] = revenue * stats['ev_revenue']['ev_revenue_q3']

        if target_financials.get('ebitda'):
            ebitda = target_financials['ebitda']
            valuations['ev_ebitda_median'] = ebitda * stats['ev_ebitda']['ev_ebitda_median']
            valuations['ev_ebitda_mean'] = ebitda * stats['ev_ebitda']['ev_ebitda_mean']
            valuations['ev_ebitda_q1'] = ebitda * stats['ev_ebitda']['ev_ebitda_q1']
            valuations['ev_ebitda_q3'] = ebitda * stats['ev_ebitda']['ev_ebitda_q3']

        if target_financials.get('ebit'):
            ebit = target_financials['ebit']
            valuations['ev_ebit_median'] = ebit * stats['ev_ebit']['ev_ebit_median']
            valuations['ev_ebit_mean'] = ebit * stats['ev_ebit']['ev_ebit_mean']

        if target_financials.get('net_income'):
            net_income = target_financials['net_income']
            valuations['pe_median'] = net_income * stats['price_earnings']['pe_median']
            valuations['pe_mean'] = net_income * stats['price_earnings']['pe_mean']

        valuation_values = [v for v in valuations.values() if v > 0]
        if valuation_values:
            valuations['blended_median'] = median(valuation_values)
            valuations['blended_mean'] = mean(valuation_values)

        return {
            'valuations': valuations,
            'multiples_used': stats,
            'comp_count': len(comps)
        }

    def build_comp_table(self, comps: List[TradingComp], target_metrics: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """Build formatted trading comp table"""

        table_data = []
        for comp in comps:
            row = {
                'Ticker': comp.ticker,
                'Company': comp.company_name,
                'Market Cap ($M)': comp.market_cap / 1_000_000,
                'EV ($M)': comp.enterprise_value / 1_000_000,
                'EV/Revenue': comp.ev_revenue,
                'EV/EBITDA': comp.ev_ebitda,
                'EV/EBIT': comp.ev_ebit,
                'P/E': comp.price_earnings,
                'P/B': comp.price_book,
                'P/S': comp.price_sales,
                'Rev Growth (%)': comp.revenue_growth,
                'EBITDA Margin (%)': comp.ebitda_margin,
                'Net Margin (%)': comp.net_margin,
                'ROE (%)': comp.roe
            }
            table_data.append(row)

        stats = self.calculate_statistics(comps)

        summary_row = {
            'Ticker': 'MEDIAN',
            'Company': '',
            'Market Cap ($M)': median([c.market_cap / 1_000_000 for c in comps]),
            'EV ($M)': median([c.enterprise_value / 1_000_000 for c in comps]),
            'EV/Revenue': stats['ev_revenue']['ev_revenue_median'],
            'EV/EBITDA': stats['ev_ebitda']['ev_ebitda_median'],
            'EV/EBIT': stats['ev_ebit']['ev_ebit_median'],
            'P/E': stats['price_earnings']['pe_median'],
            'P/B': stats['price_book']['pb_median'],
            'P/S': stats['price_sales']['ps_median'],
            'Rev Growth (%)': stats['revenue_growth']['rev_growth_median'],
            'EBITDA Margin (%)': stats['ebitda_margin']['ebitda_margin_median'],
            'Net Margin (%)': stats['net_margin']['net_margin_median'],
            'ROE (%)': stats['roe']['roe_median']
        }

        result = {
            'comparables': table_data,
            'summary_statistics': {
                'median': summary_row,
                'detailed_stats': stats
            },
            'comp_count': len(comps),
            'as_of_date': datetime.now().strftime('%Y-%m-%d')
        }

        if target_metrics:
            valuation_result = self.apply_trading_multiples(target_metrics, comps)
            result['target_valuation'] = valuation_result

        return result

    def regression_analysis(self, comps: List[TradingComp], x_metric: str = 'ebitda_margin',
                           y_metric: str = 'ev_ebitda') -> Dict[str, Any]:
        """Perform regression analysis on comp set"""

        x_vals = []
        y_vals = []

        for comp in comps:
            x = getattr(comp, x_metric, None)
            y = getattr(comp, y_metric, None)

            if x and y and x > 0 and y > 0 and not np.isnan(x) and not np.isnan(y):
                x_vals.append(x)
                y_vals.append(y)

        if len(x_vals) < 3:
            return {'error': 'Insufficient data for regression'}

        coefficients = np.polyfit(x_vals, y_vals, 1)
        slope, intercept = coefficients

        y_pred = [slope * x + intercept for x in x_vals]
        residuals = [y - yp for y, yp in zip(y_vals, y_pred)]
        ss_res = sum(r**2 for r in residuals)
        ss_tot = sum((y - mean(y_vals))**2 for y in y_vals)
        r_squared = 1 - (ss_res / ss_tot) if ss_tot > 0 else 0

        return {
            'slope': slope,
            'intercept': intercept,
            'r_squared': r_squared,
            'x_metric': x_metric,
            'y_metric': y_metric,
            'data_points': len(x_vals),
            'x_values': x_vals,
            'y_values': y_vals
        }

def main():
    """CLI entry point - outputs JSON for Tauri integration"""
    import json

    if len(sys.argv) < 2:
        result = {
            "success": False,
            "error": "No command specified. Usage: trading_comps.py <command> [args...]"
        }
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]
    analyzer = TradingCompsAnalyzer()

    try:
        if command == "trading_comps":
            # Rust sends: "trading_comps" target_ticker comp_tickers_json
            if len(sys.argv) < 4:
                raise ValueError("Target ticker and comp tickers required")

            target_ticker = sys.argv[2]
            comp_tickers = json.loads(sys.argv[3])

            # Fetch target data
            target_comp = analyzer.fetch_comp_data(target_ticker)
            target_financials = {}
            if target_comp:
                target_financials = {
                    'revenue': target_comp.revenue_ltm,
                    'ebitda': target_comp.ebitda_ltm,
                    'ebit': target_comp.ebit_ltm,
                    'net_income': target_comp.net_income_ltm
                }

            # Fetch comps
            comps = []
            for ticker in comp_tickers:
                comp = analyzer.fetch_comp_data(ticker)
                if comp and comp.market_cap > 0:
                    comps.append(comp)

            if not comps:
                result = {"success": True, "data": {"comparables": [], "comp_count": 0, "target": target_ticker}}
                print(json.dumps(result))
            else:
                comp_table = analyzer.build_comp_table(comps, target_financials if target_financials else None)
                comp_table['target_ticker'] = target_ticker
                result = {"success": True, "data": comp_table}
                print(json.dumps(result, default=str))

        elif command == "find_comps":
            # find_comparables(industry)
            if len(sys.argv) < 3:
                raise ValueError("Industry required")

            industry = sys.argv[2]
            comps = analyzer.find_comparables(industry)

            result = {
                "success": True,
                "data": comps,
                "count": len(comps)
            }
            print(json.dumps(result))

        elif command == "build_table":
            # build_comp_table(industry, target_financials)
            if len(sys.argv) < 4:
                raise ValueError("Industry and target financials required")

            industry = sys.argv[2]
            target_financials = json.loads(sys.argv[3])

            comps = analyzer.find_comparables(industry)
            comp_table = analyzer.build_comp_table(comps, target_financials)

            result = {
                "success": True,
                "data": comp_table
            }
            print(json.dumps(result))

        else:
            result = {
                "success": False,
                "error": f"Unknown command: {command}. Available: trading_comps, find_comps, build_table"
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
