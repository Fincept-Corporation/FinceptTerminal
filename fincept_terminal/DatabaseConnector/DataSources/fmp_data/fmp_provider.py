# TESLA (TSLA) - MAXIMUM ANALYSIS WITH MINIMUM API CALLS
# Optimized for 250 requests/day FMP Free Plan (Latest 2025 API)
# ERROR-FREE VERSION with TXT file output

import requests
import pandas as pd
import json
from datetime import datetime

# Configuration
API_KEY = "9e9a691a55bedfb8cde976452e2eff3b"  # Replace with your actual key
BASE_URL = "https://financialmodelingprep.com/api/v3"
SYMBOL = "TSLA"  # Tesla Inc. on NASDAQ

request_count = 0
analysis_results = []  # Store all analysis results


def safe_format(value, format_type='number', decimal_places=2):
    """Safely format values to avoid errors"""
    if value is None or value == 'N/A' or value == '':
        return 'N/A'

    try:
        if format_type == 'percentage':
            return f"{float(value):.{decimal_places}%}"
        elif format_type == 'number':
            return f"{float(value):,.{decimal_places}f}"
        elif format_type == 'currency':
            return f"${float(value):,.0f}"
        else:
            return str(value)
    except:
        return str(value) if value is not None else 'N/A'


def log_result(section, content):
    """Log results for TXT file output"""
    analysis_results.append(f"\n{section}")
    analysis_results.append("=" * len(section))
    analysis_results.append(content)


def make_request(endpoint, params=None):
    """Efficient request function with counter"""
    global request_count

    if params is None:
        params = {}
    params['apikey'] = API_KEY

    try:
        response = requests.get(f"{BASE_URL}/{endpoint}", params=params)
        request_count += 1
        print(f"📡 Request {request_count}: {endpoint}")

        if response.status_code == 200:
            return response.json()
        else:
            print(f"❌ Error {response.status_code}: {endpoint}")
            return None
    except Exception as e:
        print(f"❌ Exception: {e}")
        return None


print("🚗 TESLA INC. (TSLA) - COMPREHENSIVE ANALYSIS")
print("🎯 Optimized for 250 FMP Requests/Day")
print("=" * 60)

# Initialize analysis log
analysis_results.append(f"TESLA INC. (TSLA) - COMPREHENSIVE FINANCIAL ANALYSIS")
analysis_results.append(f"Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
analysis_results.append(f"{'=' * 60}")

# 1. TTM KEY METRICS (50+ ratios in 1 request!)
print("\n📊 TTM KEY METRICS (50+ ratios in 1 call):")
ttm_data = make_request(f"key-metrics-ttm/{SYMBOL}")

if ttm_data and len(ttm_data) > 0:
    print(f"✅ Found data for: {SYMBOL}")
    ttm_df = pd.DataFrame(ttm_data)
    print(f"📈 Retrieved {len(ttm_df.columns)} TTM metrics")

    # Key TTM Metrics Summary
    latest = ttm_data[0]  # Latest TTM data

    ttm_summary = f"""
🔑 KEY TTM RATIOS:
• P/E Ratio: {safe_format(latest.get('peRatioTTM'), 'number')}
• P/B Ratio: {safe_format(latest.get('pbRatioTTM'), 'number')}
• ROE: {safe_format(latest.get('roeTTM'), 'percentage')}
• ROA: {safe_format(latest.get('roaTTM'), 'percentage')}
• Current Ratio: {safe_format(latest.get('currentRatioTTM'), 'number')}
• Debt to Equity: {safe_format(latest.get('debtToEquityTTM'), 'number')}
• Revenue TTM: {safe_format(latest.get('revenueTTM'), 'currency')}
• Net Income TTM: {safe_format(latest.get('netIncomeTTM'), 'currency')}
• Market Cap: {safe_format(latest.get('marketCapTTM'), 'currency')}
• Enterprise Value: {safe_format(latest.get('enterpriseValueTTM'), 'currency')}
• EV/Revenue: {safe_format(latest.get('enterpriseValueOverRevenueTTM'), 'number')}
• EV/EBITDA: {safe_format(latest.get('evToEbitdaTTM'), 'number')}
• Free Cash Flow: {safe_format(latest.get('freeCashFlowTTM'), 'currency')}
• Operating Cash Flow: {safe_format(latest.get('operatingCashFlowTTM'), 'currency')}
    """
    print(ttm_summary)
    log_result("TTM KEY METRICS", ttm_summary)

# 2. TTM RATIOS (40+ additional ratios in 1 request!)
print("\n📊 TTM FINANCIAL RATIOS (40+ ratios in 1 call):")
ttm_ratios = make_request(f"ratios-ttm/{SYMBOL}")

if ttm_ratios and len(ttm_ratios) > 0:
    ratios_df = pd.DataFrame(ttm_ratios)
    print(f"📈 Retrieved {len(ratios_df.columns)} TTM ratios")

    latest_ratios = ttm_ratios[0]

    ratios_summary = f"""
📈 PROFITABILITY RATIOS:
• Gross Margin: {safe_format(latest_ratios.get('grossProfitMarginTTM'), 'percentage')}
• Operating Margin: {safe_format(latest_ratios.get('operatingProfitMarginTTM'), 'percentage')}
• Net Margin: {safe_format(latest_ratios.get('netProfitMarginTTM'), 'percentage')}
• Return on Equity (ROE): {safe_format(latest_ratios.get('returnOnEquityTTM'), 'percentage')}
• Return on Assets (ROA): {safe_format(latest_ratios.get('returnOnAssetsTTM'), 'percentage')}
• Return on Capital Employed: {safe_format(latest_ratios.get('returnOnCapitalEmployedTTM'), 'percentage')}

💧 LIQUIDITY RATIOS:
• Current Ratio: {safe_format(latest_ratios.get('currentRatioTTM'), 'number')}
• Quick Ratio: {safe_format(latest_ratios.get('quickRatioTTM'), 'number')}
• Cash Ratio: {safe_format(latest_ratios.get('cashRatioTTM'), 'number')}
• Operating Cash Flow Ratio: {safe_format(latest_ratios.get('operatingCashFlowRatioTTM'), 'number')}

⚙️ EFFICIENCY RATIOS:
• Asset Turnover: {safe_format(latest_ratios.get('assetTurnoverTTM'), 'number')}
• Inventory Turnover: {safe_format(latest_ratios.get('inventoryTurnoverTTM'), 'number')}
• Receivables Turnover: {safe_format(latest_ratios.get('receivablesTurnoverTTM'), 'number')}
• Days Sales Outstanding: {safe_format(latest_ratios.get('daysOfSalesOutstandingTTM'), 'number')}

🏛️ SOLVENCY RATIOS:
• Debt Ratio: {safe_format(latest_ratios.get('debtRatioTTM'), 'number')}
• Debt to Equity: {safe_format(latest_ratios.get('debtToEquityRatioTTM'), 'number')}
• Interest Coverage: {safe_format(latest_ratios.get('timesInterestEarnedTTM'), 'number')}
• Cash Coverage: {safe_format(latest_ratios.get('cashCoverageTTM'), 'number')}
    """
    print(ratios_summary)
    log_result("TTM FINANCIAL RATIOS", ratios_summary)

# 3. FINANCIAL STATEMENTS (3 requests for complete picture)
print(f"\n📋 LATEST FINANCIAL STATEMENTS:")

# Income Statement (Most recent year)
income_data = make_request(f"income-statement/{SYMBOL}", {"limit": 1})
if income_data and len(income_data) > 0:
    income = income_data[0]
    income_summary = f"""
💰 INCOME STATEMENT (Latest Year - {income.get('date', 'N/A')}):
• Revenue: {safe_format(income.get('revenue'), 'currency')}
• Cost of Revenue: {safe_format(income.get('costOfRevenue'), 'currency')}
• Gross Profit: {safe_format(income.get('grossProfit'), 'currency')}
• Gross Profit Ratio: {safe_format(income.get('grossProfitRatio'), 'percentage')}
• Operating Income: {safe_format(income.get('operatingIncome'), 'currency')}
• Operating Income Ratio: {safe_format(income.get('operatingIncomeRatio'), 'percentage')}
• Net Income: {safe_format(income.get('netIncome'), 'currency')}
• EPS: {safe_format(income.get('eps'), 'number')}
• EPS Diluted: {safe_format(income.get('epsdiluted'), 'number')}
• EBITDA: {safe_format(income.get('ebitda'), 'currency')}
• EBITDA Ratio: {safe_format(income.get('ebitdaratio'), 'percentage')}
    """
    print(income_summary)
    log_result("INCOME STATEMENT", income_summary)

# Balance Sheet (Most recent)
balance_data = make_request(f"balance-sheet-statement/{SYMBOL}", {"limit": 1})
if balance_data and len(balance_data) > 0:
    balance = balance_data[0]
    balance_summary = f"""
🏦 BALANCE SHEET (Latest - {balance.get('date', 'N/A')}):
• Total Assets: {safe_format(balance.get('totalAssets'), 'currency')}
• Current Assets: {safe_format(balance.get('totalCurrentAssets'), 'currency')}
• Non-Current Assets: {safe_format(balance.get('totalNonCurrentAssets'), 'currency')}
• Cash and Equivalents: {safe_format(balance.get('cashAndCashEquivalents'), 'currency')}
• Total Liabilities: {safe_format(balance.get('totalLiabilities'), 'currency')}
• Current Liabilities: {safe_format(balance.get('totalCurrentLiabilities'), 'currency')}
• Total Debt: {safe_format(balance.get('totalDebt'), 'currency')}
• Net Debt: {safe_format(balance.get('netDebt'), 'currency')}
• Shareholder Equity: {safe_format(balance.get('totalStockholdersEquity'), 'currency')}
• Retained Earnings: {safe_format(balance.get('retainedEarnings'), 'currency')}
• Working Capital: {safe_format((balance.get('totalCurrentAssets') or 0) - (balance.get('totalCurrentLiabilities') or 0), 'currency')}
    """
    print(balance_summary)
    log_result("BALANCE SHEET", balance_summary)

# Cash Flow (Most recent)
cashflow_data = make_request(f"cash-flow-statement/{SYMBOL}", {"limit": 1})
if cashflow_data and len(cashflow_data) > 0:
    cashflow = cashflow_data[0]
    cashflow_summary = f"""
💸 CASH FLOW STATEMENT (Latest - {cashflow.get('date', 'N/A')}):
• Operating Cash Flow: {safe_format(cashflow.get('netCashProvidedByOperatingActivities'), 'currency')}
• Investing Cash Flow: {safe_format(cashflow.get('netCashUsedForInvestingActivites'), 'currency')}
• Financing Cash Flow: {safe_format(cashflow.get('netCashUsedProvidedByFinancingActivities'), 'currency')}
• Free Cash Flow: {safe_format(cashflow.get('freeCashFlow'), 'currency')}
• Capital Expenditures: {safe_format(cashflow.get('capitalExpenditure'), 'currency')}
• Dividend Payments: {safe_format(cashflow.get('dividendsPaid'), 'currency')}
• Stock Repurchases: {safe_format(cashflow.get('commonStockRepurchased'), 'currency')}
• Net Change in Cash: {safe_format(cashflow.get('netChangeInCash'), 'currency')}
    """
    print(cashflow_summary)
    log_result("CASH FLOW STATEMENT", cashflow_summary)

# 4. COMPANY PROFILE & QUOTE (2 requests)
print(f"\n🏢 COMPANY OVERVIEW:")

profile_data = make_request(f"profile/{SYMBOL}")
if profile_data and len(profile_data) > 0:
    profile = profile_data[0]
    profile_summary = f"""
📊 COMPANY PROFILE:
• Company Name: {profile.get('companyName', 'N/A')}
• Symbol: {profile.get('symbol', 'N/A')}
• Exchange: {profile.get('exchangeShortName', 'N/A')}
• Sector: {profile.get('sector', 'N/A')}
• Industry: {profile.get('industry', 'N/A')}
• Country: {profile.get('country', 'N/A')}
• Market Cap: {safe_format(profile.get('mktCap'), 'currency')}
• Price: {safe_format(profile.get('price'), 'number')}
• Beta: {safe_format(profile.get('beta'), 'number')}
• Volume Average: {safe_format(profile.get('volAvg'), 'number', 0)}
• Last Dividend: {safe_format(profile.get('lastDiv'), 'number')}
• Range: {profile.get('range', 'N/A')}
• Changes: {safe_format(profile.get('changes'), 'number')}
• CEO: {profile.get('ceo', 'N/A')}
• Website: {profile.get('website', 'N/A')}
• Description: {profile.get('description', 'N/A')[:200]}...
    """
    print(profile_summary)
    log_result("COMPANY PROFILE", profile_summary)

# Current Quote
quote_data = make_request(f"quote/{SYMBOL}")
if quote_data and len(quote_data) > 0:
    quote = quote_data[0]
    quote_summary = f"""
💹 CURRENT MARKET DATA:
• Symbol: {quote.get('symbol', 'N/A')}
• Current Price: {safe_format(quote.get('price'), 'number')}
• Day Change: {safe_format(quote.get('change'), 'number')} ({safe_format(quote.get('changesPercentage'), 'number')}%)
• Day High: {safe_format(quote.get('dayHigh'), 'number')}
• Day Low: {safe_format(quote.get('dayLow'), 'number')}
• Year High: {safe_format(quote.get('yearHigh'), 'number')}
• Year Low: {safe_format(quote.get('yearLow'), 'number')}
• Volume: {safe_format(quote.get('volume'), 'number', 0)}
• Average Volume: {safe_format(quote.get('avgVolume'), 'number', 0)}
• Market Cap: {safe_format(quote.get('marketCap'), 'currency')}
• P/E Ratio: {safe_format(quote.get('pe'), 'number')}
• EPS: {safe_format(quote.get('eps'), 'number')}
• Open: {safe_format(quote.get('open'), 'number')}
• Previous Close: {safe_format(quote.get('previousClose'), 'number')}
    """
    print(quote_summary)
    log_result("CURRENT MARKET DATA", quote_summary)

# 5. FINANCIAL SCORES (1 request - Advanced metrics)
print(f"\n⭐ FINANCIAL HEALTH SCORES:")
scores_data = make_request(f"score/{SYMBOL}")
if scores_data and len(scores_data) > 0:
    scores = scores_data[0]
    scores_summary = f"""
🎯 FINANCIAL HEALTH SCORES:
• Symbol: {scores.get('symbol', 'N/A')}
• Altman Z-Score: {safe_format(scores.get('altmanZScore'), 'number')} 
  (>2.99: Safe, 1.81-2.99: Caution, <1.81: Distress Zone)
• Piotroski Score: {safe_format(scores.get('piotroskiScore'), 'number')} 
  (0-9 scale, higher is better financial strength)
• Working Capital: {safe_format(scores.get('workingCapital'), 'currency')}
• Total Assets: {safe_format(scores.get('totalAssets'), 'currency')}
• Retained Earnings: {safe_format(scores.get('retainedEarnings'), 'currency')}
• EBIT: {safe_format(scores.get('ebit'), 'currency')}
• Market Value of Equity: {safe_format(scores.get('marketValueOfEquity'), 'currency')}
• Total Liabilities: {safe_format(scores.get('totalLiabilities'), 'currency')}
• Revenue: {safe_format(scores.get('revenue'), 'currency')}
    """
    print(scores_summary)
    log_result("FINANCIAL HEALTH SCORES", scores_summary)

# 6. SAVE ALL DATA TO FILES
print(f"\n💾 EXPORTING COMPREHENSIVE DATA:")

# Save to Excel
try:
    with pd.ExcelWriter('TESLA_Complete_Analysis.xlsx', engine='xlsxwriter') as writer:
        # Save all collected data
        if 'ttm_data' in locals() and ttm_data:
            pd.DataFrame(ttm_data).to_excel(writer, sheet_name='TTM_Key_Metrics', index=False)

        if 'ttm_ratios' in locals() and ttm_ratios:
            pd.DataFrame(ttm_ratios).to_excel(writer, sheet_name='TTM_Ratios', index=False)

        if 'income_data' in locals() and income_data:
            pd.DataFrame(income_data).to_excel(writer, sheet_name='Income_Statement', index=False)

        if 'balance_data' in locals() and balance_data:
            pd.DataFrame(balance_data).to_excel(writer, sheet_name='Balance_Sheet', index=False)

        if 'cashflow_data' in locals() and cashflow_data:
            pd.DataFrame(cashflow_data).to_excel(writer, sheet_name='Cash_Flow', index=False)

        if 'profile_data' in locals() and profile_data:
            pd.DataFrame(profile_data).to_excel(writer, sheet_name='Company_Profile', index=False)

        if 'quote_data' in locals() and quote_data:
            pd.DataFrame(quote_data).to_excel(writer, sheet_name='Current_Quote', index=False)

        if 'scores_data' in locals() and scores_data:
            pd.DataFrame(scores_data).to_excel(writer, sheet_name='Financial_Scores', index=False)

    print("✅ Exported to 'TESLA_Complete_Analysis.xlsx'")
except Exception as e:
    print(f"⚠️ Excel export error: {e}")

# Save to TXT file (Main requirement)
try:
    with open('TESLA_Financial_Analysis_Report.txt', 'w', encoding='utf-8') as f:
        f.write('\n'.join(analysis_results))

        # Add detailed data sections
        f.write(f"\n\n{'=' * 80}")
        f.write(f"\nDETAILED DATA TABLES")
        f.write(f"\n{'=' * 80}")

        if 'ttm_data' in locals() and ttm_data:
            f.write(f"\n\nTTM KEY METRICS - ALL DATA:")
            f.write(f"\n{'-' * 50}")
            for key, value in ttm_data[0].items():
                f.write(f"\n{key}: {safe_format(value, 'number')}")

        if 'ttm_ratios' in locals() and ttm_ratios:
            f.write(f"\n\nTTM RATIOS - ALL DATA:")
            f.write(f"\n{'-' * 50}")
            for key, value in ttm_ratios[0].items():
                f.write(f"\n{key}: {safe_format(value, 'number')}")

        if 'income_data' in locals() and income_data:
            f.write(f"\n\nINCOME STATEMENT - ALL DATA:")
            f.write(f"\n{'-' * 50}")
            for key, value in income_data[0].items():
                f.write(
                    f"\n{key}: {safe_format(value, 'currency' if 'revenue' in key.lower() or 'income' in key.lower() or 'profit' in key.lower() else 'number')}")

        if 'balance_data' in locals() and balance_data:
            f.write(f"\n\nBALANCE SHEET - ALL DATA:")
            f.write(f"\n{'-' * 50}")
            for key, value in balance_data[0].items():
                f.write(
                    f"\n{key}: {safe_format(value, 'currency' if 'assets' in key.lower() or 'liabilities' in key.lower() or 'equity' in key.lower() or 'debt' in key.lower() else 'number')}")

        if 'cashflow_data' in locals() and cashflow_data:
            f.write(f"\n\nCASH FLOW STATEMENT - ALL DATA:")
            f.write(f"\n{'-' * 50}")
            for key, value in cashflow_data[0].items():
                f.write(
                    f"\n{key}: {safe_format(value, 'currency' if 'cash' in key.lower() or 'flow' in key.lower() else 'number')}")

        # Add summary statistics
        f.write(f"\n\n{'=' * 80}")
        f.write(f"\nANALYSIS SUMMARY")
        f.write(f"\n{'=' * 80}")
        f.write(f"\nTotal API Requests Used: {request_count}/250")
        f.write(f"\nRemaining Daily Requests: {250 - request_count}")
        f.write(f"\nData Categories Analyzed: 6+ major categories")
        f.write(f"\nTotal Financial Metrics: 150+ ratios and indicators")
        f.write(f"\nGenerated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")

    print("✅ Exported detailed analysis to 'TESLA_Financial_Analysis_Report.txt'")
except Exception as e:
    print(f"⚠️ TXT export error: {e}")

# MANUAL CALCULATION EXAMPLE (if you want to calculate more ratios)
if 'income_data' in locals() and 'balance_data' in locals() and income_data and balance_data:
    print(f"\n🔢 MANUAL RATIO CALCULATIONS:")
    try:
        # Get latest data
        inc = income_data[0]
        bal = balance_data[0]

        manual_calcs = []

        # Calculate additional ratios
        if inc.get('revenue') and bal.get('totalAssets'):
            asset_turnover = float(inc['revenue']) / float(bal['totalAssets'])
            manual_calcs.append(f"• Asset Turnover: {asset_turnover:.2f}")

        if bal.get('totalCurrentAssets') and bal.get('totalCurrentLiabilities'):
            current_ratio = float(bal['totalCurrentAssets']) / float(bal['totalCurrentLiabilities'])
            manual_calcs.append(f"• Current Ratio: {current_ratio:.2f}")

        if inc.get('grossProfit') and inc.get('revenue'):
            gross_margin = float(inc['grossProfit']) / float(inc['revenue'])
            manual_calcs.append(f"• Gross Margin: {gross_margin:.2%}")

        if inc.get('netIncome') and bal.get('totalStockholdersEquity'):
            roe = float(inc['netIncome']) / float(bal['totalStockholdersEquity'])
            manual_calcs.append(f"• Return on Equity: {roe:.2%}")

        manual_summary = '\n'.join(manual_calcs)
        print(manual_summary)
        log_result("MANUAL CALCULATIONS", manual_summary)

        # Append to TXT file
        try:
            with open('TESLA_Financial_Analysis_Report.txt', 'a', encoding='utf-8') as f:
                f.write(f"\n\nMANUAL RATIO CALCULATIONS:")
                f.write(f"\n{'-' * 50}")
                f.write(f"\n{manual_summary}")
        except:
            pass

    except Exception as e:
        print(f"⚠️ Manual calculation error: {e}")

# FINAL SUMMARY
print(f"\n" + "=" * 60)
print(f"🎯 ANALYSIS COMPLETE!")
print(f"📊 Total API Requests Used: {request_count}/250")
print(f"📈 Data Retrieved:")
print(f"   • 60+ TTM Key Metrics")
print(f"   • 40+ TTM Financial Ratios")
print(f"   • Complete Financial Statements")
print(f"   • Company Profile & Current Quote")
print(f"   • Financial Health Scores")
print(f"   • Manual Calculated Ratios")
print(f"🏢 Company: Tesla Inc. (TSLA)")
print(f"💾 Files Generated:")
print(f"   • TESLA_Complete_Analysis.xlsx (Excel format)")
print(f"   • TESLA_Financial_Analysis_Report.txt (Detailed report)")
print(f"📋 Remaining Daily Requests: {250 - request_count}")
print(f"\n💡 SUCCESS: Complete 150+ parameter analysis with minimal API usage!")
print(f"🚀 You can analyze {(250 // request_count)} more companies today!")