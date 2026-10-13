"""Equity screener backed by the Yahoo Finance screener API.

yfinance is used only for its Yahoo session handling (cookie + crumb); the request bodies are
built here so the screen can ask for the fields a professional screen needs.

CLI — JSON on stdout; on failure ``{"error": "..."}`` on stdout and exit code 1; diagnostics on
stderr only:

    python screener_data.py predefined <screen_id> [count] [offset]
    python screener_data.py custom <criteria-json | -> [count] [sort_field] [asc|desc] [offset]
    python screener_data.py fields

``custom`` criteria (``-`` reads the JSON document from stdin)::

    {
      "region":     ["us"],                  # Yahoo region codes        (see `fields`)
      "exchange":   ["NMS", "NYQ"],          # Yahoo exchange codes      (see `fields`)
      "sector":     ["Technology"],
      "industry":   ["Semiconductors"],
      "peer_group": ["Banks"],
      "filters": [
        {"field": "market_cap", "op": "gte",  "value": 2e9},
        {"field": "pe",         "op": "btwn", "value": [0, 20]}
      ],
      "count": 50, "offset": 0, "sort_field": "market_cap", "sort_asc": false   # optional; argv wins
    }

Each run makes two Yahoo calls:
  1. the screen itself (quote format): the ordered result set, the match total and the current
     (exchange-delayed) quote fields — price, change %, volume, market cap, P/E;
  2. a records lookup for exactly those tickers: sector, industry, beta and the requested
     criteria fields, which the quote format does not carry.
Yahoo evaluates criteria on its screener snapshot, which lags the live quote. A row's current
price / % change can therefore sit slightly outside a price or % change bound; the output says so.

Units (verified against live responses, Oct 2026):
  * percent fields are percent points (25 = 25 %);
  * price and market-cap criteria compare in the listing's QUOTE currency — for listings quoted in
    a minor unit (LSE GBp, JSE ZAc, TASE ILA, Kuwait KWF) Yahoo compares in that minor unit;
  * fundamentals (revenue, EBITDA, debt, FCF, ...) are in the company's REPORTING currency;
  * the market cap returned per row is in the major unit (GBP for a GBp listing), reported as
    ``market_cap_currency``.
"""

import json
import math
import os
import sys
import time

try:
    import fincept_json
except ImportError:  # scripts dir not on sys.path (unusual launcher)
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    import fincept_json

SOURCE = "Yahoo Finance screener"
SCREENER_URL = "https://query1.finance.yahoo.com/v1/finance/screener"
PREDEFINED_URL = SCREENER_URL + "/predefined/saved"
MAX_COUNT = 250  # Yahoo's hard limit per request
DEFAULT_COUNT = 50
DEFAULT_SORT = "market_cap"

try:
    NET_TIMEOUT = float(os.environ.get("FINCEPT_YF_TIMEOUT", "20"))
except (TypeError, ValueError):
    NET_TIMEOUT = 20.0

# ── Field catalogue ──────────────────────────────────────────────────────────
# (id, label, yahoo field, records-response key, category, unit, description)
# Every yahoo field below was verified as a valid screener operand AND include-field.
# yfinance 0.2.66's own list has one bogus entry ("netepsbasic.lasttwelvemonthsnetepsdiluted...")
# that Yahoo rejects — it is deliberately absent.
FIELDS = [
    # Price & performance
    ("price", "Price", "intradayprice", "regularMarketPrice", "Price & Performance", "price",
     "Last price in the listing's quote currency."),
    ("change_pct", "Change %", "percentchange", "regularMarketChangePercent", "Price & Performance", "pct",
     "Change vs. previous close, in percent."),
    ("change", "Change", "intradaypricechange", "regularMarketChange", "Price & Performance", "price",
     "Price change vs. previous close."),
    ("week52_change_pct", "52W Change %", "fiftytwowkpercentchange", "fiftyTwoWeekChangePercent",
     "Price & Performance", "pct", "Price change over the last 52 weeks, in percent."),
    ("week52_high", "52W High (close)", "lastclose52weekhigh.lasttwelvemonths", "lastClose52WeekHighLtm",
     "Price & Performance", "price", "Highest close of the last 52 weeks."),
    ("week52_low", "52W Low (close)", "lastclose52weeklow.lasttwelvemonths", "lastClose52WeekLowLtm",
     "Price & Performance", "price", "Lowest close of the last 52 weeks."),
    # Size
    ("market_cap", "Market Cap", "intradaymarketcap", "marketCap", "Size", "mcap",
     "Market capitalisation, compared in the listing's quote currency (minor unit for GBp/ZAc/ILA/KWF listings)."),
    ("shares_out", "Shares Outstanding", "totalsharesoutstanding", "totalSharesOutstanding", "Size", "shares",
     "Total shares outstanding."),
    # Volume & ownership
    ("volume", "Volume", "dayvolume", "regularMarketVolume", "Volume & Ownership", "shares",
     "Shares traded in the current session."),
    ("avg_volume_3m", "Avg Volume (3M)", "avgdailyvol3m", "avgDailyVol3m", "Volume & Ownership", "shares",
     "Average daily volume over 3 months."),
    ("eod_volume", "Prior-Session Volume", "eodvolume", "eodVolume", "Volume & Ownership", "shares",
     "Volume of the last completed session (Yahoo's default sort for several screens)."),
    ("beta", "Beta", "beta", "beta", "Volume & Ownership", "ratio", "Beta vs. the local benchmark."),
    ("inst_own_pct", "Institutional Ownership %", "pctheldinst", "percentOfSharesOutHeldByInstitutions",
     "Volume & Ownership", "pct", "Shares held by institutions, % of shares outstanding."),
    ("insider_own_pct", "Insider Ownership %", "pctheldinsider", "percentOfSharesOutHeldByInsider",
     "Volume & Ownership", "pct", "Shares held by insiders, % of shares outstanding."),
    # Short interest
    ("short_pct_float", "Short % of Float", "short_percentage_of_float.value", "shortPercentageOfFloat",
     "Short Interest", "pct", "Short interest as % of float (exchange reports, twice monthly)."),
    ("short_pct_out", "Short % of Shares Out", "short_percentage_of_shares_outstanding.value",
     "shortPercentageOfSharesOutstanding", "Short Interest", "pct",
     "Short interest as % of shares outstanding."),
    ("days_to_cover", "Days to Cover", "days_to_cover_short.value", "daysToCoverShort", "Short Interest", "days",
     "Short interest / average daily volume."),
    ("short_interest", "Short Interest (shares)", "short_interest.value", "shortInterest", "Short Interest",
     "shares", "Shares sold short."),
    ("short_interest_chg_pct", "Short Interest Change %", "short_interest_percentage_change.value",
     "shortInterestPercentageChange", "Short Interest", "pct", "Change in short interest since the prior report."),
    # Valuation
    ("pe", "P/E (TTM)", "peratio.lasttwelvemonths", "peRatioLtm", "Valuation", "ratio",
     "Price / trailing-twelve-month EPS."),
    ("peg", "PEG (5Y)", "pegratio_5y", "pegRatio5Yr", "Valuation", "ratio", "P/E divided by 5-year expected growth."),
    ("pb", "P/B", "pricebookratio.quarterly", "priceBookRatioQuarterly", "Valuation", "ratio",
     "Price / book value per share (latest quarter)."),
    ("ps", "P/S (TTM)", "lastclosemarketcaptotalrevenue.lasttwelvemonths", "priceSalesRatioLtm", "Valuation",
     "ratio", "Market cap / trailing-twelve-month revenue."),
    ("p_tbv", "Price / Tangible Book", "lastclosepricetangiblebookvalue.lasttwelvemonths",
     "lastClosePriceTangibleBookValueLtm", "Valuation", "ratio", "Price / tangible book value per share."),
    ("ev_revenue", "EV / Revenue", "lastclosetevtotalrevenue.lasttwelvemonths", "lastCloseTevTotalRevenueLtm",
     "Valuation", "ratio", "Enterprise value / TTM revenue."),
    ("ev_ebitda", "EV / EBITDA", "lastclosetevebitda.lasttwelvemonths", "lastCloseTevEbitdaLtm", "Valuation",
     "ratio", "Enterprise value / TTM EBITDA."),
    ("ev_ebit", "EV / EBIT", "lastclosetevebit.lasttwelvemonths", "lastCloseTevEbitLtm", "Valuation", "ratio",
     "Enterprise value / TTM EBIT."),
    ("book_value_ps", "Book Value / Share", "bookvalueshare.lasttwelvemonths", "bookValueShareLtm", "Valuation",
     "per_share", "Book value per share, reporting currency."),
    # Dividends
    ("dividend_yield", "Fwd Dividend Yield %", "forward_dividend_yield", "forwardDividendYieldPercent",
     "Dividends", "pct", "Forward annual dividend / price, in percent."),
    ("dividend_ps", "Fwd Dividend / Share", "forward_dividend_per_share", "forwardDividendPerShare", "Dividends",
     "per_share", "Forward annual dividend per share."),
    ("dividend_growth_years", "Dividend Growth Streak (yrs)", "consecutive_years_of_dividend_growth_count",
     "consecutiveYearsOfDividendGrowthCount", "Dividends", "years", "Consecutive years of dividend growth."),
    # Profitability
    ("roe", "ROE %", "returnonequity.lasttwelvemonths", "returnOnEquityPercentLtm", "Profitability", "pct",
     "Return on equity, TTM."),
    ("roa", "ROA %", "returnonassets.lasttwelvemonths", "returnOnAssetsPercentLtm", "Profitability", "pct",
     "Return on assets, TTM."),
    ("rotc", "Return on Total Capital %", "returnontotalcapital.lasttwelvemonths", "returnOnTotalCapitalLtm",
     "Profitability", "pct", "Return on total capital, TTM."),
    ("gross_margin", "Gross Margin %", "grossprofitmargin.lasttwelvemonths", "grossProfitMarginPercentLtm",
     "Profitability", "pct", "Gross profit / revenue, TTM."),
    ("ebitda_margin", "EBITDA Margin %", "ebitdamargin.lasttwelvemonths", "ebitdaMarginPercentLtm",
     "Profitability", "pct", "EBITDA / revenue, TTM."),
    ("net_margin", "Net Margin %", "netincomemargin.lasttwelvemonths", "netIncomeMarginPercentLtm",
     "Profitability", "pct", "Net income / revenue, TTM."),
    # Growth
    ("eps_growth", "EPS Growth % (TTM YoY)", "epsgrowth.lasttwelvemonths", "epsGrowthPercentLtm", "Growth", "pct",
     "Year-over-year growth of trailing EPS."),
    ("diluted_eps_growth", "Diluted EPS 1Y Growth %", "dilutedeps1yrgrowth.lasttwelvemonths",
     "dilutedEps1YrGrowthPercentLtm", "Growth", "pct", "One-year growth of diluted EPS."),
    ("revenue_growth_q", "Revenue Growth % (Qtr YoY)", "quarterlyrevenuegrowth.quarterly",
     "revenueGrowthPercentYoYQuarterly", "Growth", "pct", "Latest quarter revenue vs. the same quarter a year ago."),
    ("revenue_growth_1y", "Revenue 1Y Growth %", "totalrevenues1yrgrowth.lasttwelvemonths",
     "totalRevenue1YrGrowthPercentLtm", "Growth", "pct", "One-year growth of TTM revenue."),
    ("net_income_growth_1y", "Net Income 1Y Growth %", "netincome1yrgrowth.lasttwelvemonths",
     "netIncome1YrGrowthPercentLtm", "Growth", "pct", "One-year growth of TTM net income."),
    ("ebitda_growth_1y", "EBITDA 1Y Growth %", "ebitda1yrgrowth.lasttwelvemonths", "ebitda1YrGrowthPercentLtm",
     "Growth", "pct", "One-year growth of TTM EBITDA."),
    ("fcf_growth_1y", "Levered FCF 1Y Growth %", "leveredfreecashflow1yrgrowth.lasttwelvemonths",
     "leveredFreeCashFlow1YrGrowthPercentLtm", "Growth", "pct", "One-year growth of levered free cash flow."),
    ("cfo_growth_1y", "Operating CF 1Y Growth %", "cashfromoperations1yrgrowth.lasttwelvemonths",
     "cashFromOperations1YrGrowthPercentLtm", "Growth", "pct", "One-year growth of cash from operations."),
    # Income statement (TTM)
    ("revenue", "Revenue (TTM)", "totalrevenues.lasttwelvemonths", "totalRevenueLtm", "Income Statement", "money",
     "Trailing-twelve-month revenue, reporting currency."),
    ("gross_profit", "Gross Profit (TTM)", "grossprofit.lasttwelvemonths", "grossProfitLtm", "Income Statement",
     "money", "TTM gross profit, reporting currency."),
    ("ebitda", "EBITDA (TTM)", "ebitda.lasttwelvemonths", "ebitdaLtm", "Income Statement", "money",
     "TTM EBITDA, reporting currency."),
    ("ebit", "EBIT (TTM)", "ebit.lasttwelvemonths", "ebitLtm", "Income Statement", "money",
     "TTM EBIT, reporting currency."),
    ("operating_income", "Operating Income (TTM)", "operatingincome.lasttwelvemonths", "operatingIncomeLtm",
     "Income Statement", "money", "TTM operating income, reporting currency."),
    ("net_income", "Net Income (TTM)", "netincomeis.lasttwelvemonths", "netIncomeIsLtm", "Income Statement",
     "money", "TTM net income, reporting currency."),
    ("eps_diluted", "Diluted EPS (TTM)", "dilutedepscontinuingoperations.lasttwelvemonths",
     "dilutedEpsContinuingOperationsLtm", "Income Statement", "per_share",
     "TTM diluted EPS from continuing operations, reporting currency."),
    ("eps_basic", "Basic EPS (TTM)", "basicepscontinuingoperations.lasttwelvemonths",
     "basicEpsContinuingOperationsLtm", "Income Statement", "per_share",
     "TTM basic EPS from continuing operations, reporting currency."),
    # Balance sheet
    ("total_assets", "Total Assets", "totalassets.lasttwelvemonths", "totalAssetsLtm", "Balance Sheet", "money",
     "Total assets, reporting currency."),
    ("total_debt", "Total Debt", "totaldebt.lasttwelvemonths", "totalDebtLtm", "Balance Sheet", "money",
     "Total debt, reporting currency."),
    ("cash", "Cash & ST Investments", "totalcashandshortterminvestments.lasttwelvemonths",
     "totalCashAndShortTermInvestmentsLtm", "Balance Sheet", "money",
     "Cash and short-term investments, reporting currency."),
    ("total_equity", "Total Equity", "totalequity.lasttwelvemonths", "totalEquityLtm", "Balance Sheet", "money",
     "Total equity, reporting currency."),
    # Cash flow
    ("cfo", "Operating Cash Flow (TTM)", "cashfromoperations.lasttwelvemonths", "cashFromOperationsLtm",
     "Cash Flow", "money", "TTM cash from operations, reporting currency."),
    ("levered_fcf", "Levered FCF (TTM)", "leveredfreecashflow.lasttwelvemonths", "leveredFreeCashFlowLtm",
     "Cash Flow", "money", "TTM levered free cash flow, reporting currency."),
    ("unlevered_fcf", "Unlevered FCF (TTM)", "unleveredfreecashflow.lasttwelvemonths", "unleveredFreeCashFlowLtm",
     "Cash Flow", "money", "TTM unlevered free cash flow, reporting currency."),
    ("capex", "CapEx (TTM)", "capitalexpenditure.lasttwelvemonths", "capitalExpenditureLtm", "Cash Flow", "money",
     "TTM capital expenditure (negative = outflow), reporting currency."),
    # Leverage & liquidity
    ("debt_equity_pct", "Total Debt / Equity %", "totaldebtequity.lasttwelvemonths", "totalDebtEquityPercentLtm",
     "Leverage & Liquidity", "pct", "Total debt / total equity, in percent (150 = 1.5x)."),
    ("lt_debt_equity_pct", "LT Debt / Equity %", "ltdebtequity.lasttwelvemonths", "longTermDebtEquityPercentLtm",
     "Leverage & Liquidity", "pct", "Long-term debt / total equity, in percent."),
    ("net_debt_ebitda", "Net Debt / EBITDA", "netdebtebitda.lasttwelvemonths", "netDebtEbitdaLtm",
     "Leverage & Liquidity", "ratio", "Net debt / TTM EBITDA (negative = net cash)."),
    ("debt_ebitda", "Total Debt / EBITDA", "totaldebtebitda.lasttwelvemonths", "totalDebtEbitdaLtm",
     "Leverage & Liquidity", "ratio", "Total debt / TTM EBITDA."),
    ("interest_coverage", "EBIT / Interest", "ebitinterestexpense.lasttwelvemonths", "ebitInterestExpenseLtm",
     "Leverage & Liquidity", "ratio", "EBIT / interest expense (interest coverage)."),
    ("ebitda_interest", "EBITDA / Interest", "ebitdainterestexpense.lasttwelvemonths",
     "ebitdaInterestExpenseLtm", "Leverage & Liquidity", "ratio", "EBITDA / interest expense."),
    ("current_ratio", "Current Ratio", "currentratio.lasttwelvemonths", "currentRatioLtm", "Leverage & Liquidity",
     "ratio", "Current assets / current liabilities."),
    ("quick_ratio", "Quick Ratio", "quickratio.lasttwelvemonths", "quickRatioLtm", "Leverage & Liquidity", "ratio",
     "(Current assets - inventory) / current liabilities."),
    ("ocf_current_liab", "Operating CF / Current Liabilities",
     "operatingcashflowtocurrentliabilities.lasttwelvemonths", "operatingCashFlowToCurrentLiabilitiesLtm",
     "Leverage & Liquidity", "ratio", "Cash from operations / current liabilities."),
    ("altman_z", "Altman Z-Score", "altmanzscoreusingtheaveragestockinformationforaperiod.lasttwelvemonths",
     "altmanZScoreLtm", "Leverage & Liquidity", "score", "Altman Z-score (below ~1.8 = distress zone)."),
    # ESG (Sustainalytics risk scores: lower = less risk)
    ("esg_score", "ESG Risk Score", "esg_score", "esgScore", "ESG", "score", "Total ESG risk (lower is better)."),
    ("environmental_score", "Environmental Risk", "environmental_score", "environmentalScore", "ESG", "score",
     "Environmental risk (lower is better)."),
    ("social_score", "Social Risk", "social_score", "socialScore", "ESG", "score", "Social risk (lower is better)."),
    ("governance_score", "Governance Risk", "governance_score", "governanceScore", "ESG", "score",
     "Governance risk (lower is better)."),
    ("highest_controversy", "Highest Controversy", "highest_controversy", "highestControversy", "ESG", "score",
     "Highest controversy level, 0 (none) to 5 (severe)."),
]

FIELD_BY_ID = {f[0]: f for f in FIELDS}
FIELD_BY_YAHOO = {f[2]: f for f in FIELDS}
CATEGORIES = list(dict.fromkeys(f[4] for f in FIELDS))

# Always shown by the UI; their values come from the live quote, so they are never "extra" columns.
BASE_COLUMNS = ("price", "change_pct", "volume", "market_cap", "pe")

OPERATORS = [
    {"id": "gt", "label": ">"},
    {"id": "gte", "label": ">="},
    {"id": "lt", "label": "<"},
    {"id": "lte", "label": "<="},
    {"id": "btwn", "label": "between"},
]
_OP_ALIASES = {"gt": "GT", ">": "GT", "gte": "GTE", ">=": "GTE", "lt": "LT", "<": "LT", "lte": "LTE",
               "<=": "LTE", "btwn": "BTWN", "between": "BTWN", "eq": "EQ", "=": "EQ"}

# Regions that return rows (probed: co, lk, pe, ph, pk, ru, sr, sw, vn return none and are omitted).
REGIONS = [
    ("us", "United States"), ("ca", "Canada"), ("mx", "Mexico"), ("br", "Brazil"), ("ar", "Argentina"),
    ("cl", "Chile"), ("ve", "Venezuela"),
    ("gb", "United Kingdom"), ("de", "Germany"), ("fr", "France"), ("it", "Italy"), ("es", "Spain"),
    ("nl", "Netherlands"), ("be", "Belgium"), ("ch", "Switzerland"), ("at", "Austria"), ("ie", "Ireland"),
    ("pt", "Portugal"), ("se", "Sweden"), ("dk", "Denmark"), ("no", "Norway"), ("fi", "Finland"),
    ("is", "Iceland"), ("pl", "Poland"), ("cz", "Czechia"), ("hu", "Hungary"), ("ro", "Romania"),
    ("gr", "Greece"), ("tr", "Turkey"), ("ee", "Estonia"), ("lv", "Latvia"), ("lt", "Lithuania"),
    ("il", "Israel"), ("sa", "Saudi Arabia"), ("qa", "Qatar"), ("kw", "Kuwait"), ("eg", "Egypt"),
    ("za", "South Africa"),
    ("in", "India"), ("cn", "China"), ("hk", "Hong Kong"), ("tw", "Taiwan"), ("jp", "Japan"),
    ("kr", "South Korea"), ("sg", "Singapore"), ("my", "Malaysia"), ("th", "Thailand"), ("id", "Indonesia"),
    ("au", "Australia"), ("nz", "New Zealand"),
]

# Exchange choices per region: (label, [codes]). Codes that returned no rows (BER, FKA, SAP, PHP,
# PHS, CXI, OEM, YHD) are omitted.
_US_MAJOR = ["NMS", "NGM", "NCM", "NYQ", "ASE"]
EXCHANGES = {
    "us": [("NASDAQ + NYSE + NYSE American (excl. OTC)", _US_MAJOR),
           ("NASDAQ (all tiers)", ["NMS", "NGM", "NCM"]),
           ("NASDAQ Global Select", ["NMS"]), ("NASDAQ Global Market", ["NGM"]),
           ("NASDAQ Capital Market", ["NCM"]), ("NYSE", ["NYQ"]), ("NYSE American", ["ASE"]),
           ("NYSE Arca", ["PCX"]), ("OTC Markets (Pink / QB / QX)", ["PNK", "OQB", "OQX"])],
    "ca": [("Toronto (TSX)", ["TOR"]), ("TSX Venture", ["VAN"]), ("Cboe Canada", ["NEO"]),
           ("Canadian Securities Exchange", ["CNQ"])],
    "mx": [("Mexico (BMV)", ["MEX"])], "br": [("B3 (Sao Paulo)", ["SAO"])],
    "ar": [("Buenos Aires", ["BUE"])], "cl": [("Santiago", ["SGO"])], "ve": [("Caracas", ["CCS"])],
    "gb": [("London (LSE)", ["LSE"]), ("LSE International Order Book", ["IOB"]), ("Aquis", ["AQS"])],
    "de": [("XETRA", ["GER"]), ("Frankfurt", ["FRA"]), ("Stuttgart", ["STU"]), ("Dusseldorf", ["DUS"]),
           ("Munich", ["MUN"]), ("Hamburg", ["HAM"])],
    "fr": [("Euronext Paris", ["PAR"])], "it": [("Borsa Italiana (Milan)", ["MIL"])],
    "es": [("Madrid (BME)", ["MCE"])], "nl": [("Euronext Amsterdam", ["AMS"])],
    "be": [("Euronext Brussels", ["BRU"])], "ch": [("SIX Swiss Exchange", ["EBS"])],
    "at": [("Vienna", ["VIE"])], "ie": [("Euronext Dublin", ["ISE"])], "pt": [("Euronext Lisbon", ["LIS"])],
    "se": [("Nasdaq Stockholm", ["STO"])], "dk": [("Nasdaq Copenhagen", ["CPH"])],
    "no": [("Oslo Bors", ["OSL"])], "fi": [("Nasdaq Helsinki", ["HEL"])], "is": [("Nasdaq Iceland", ["ICE"])],
    "pl": [("Warsaw (GPW)", ["WSE"])], "cz": [("Prague", ["PRA"])], "hu": [("Budapest", ["BUD"])],
    "ro": [("Bucharest (BVB)", ["BVB"])], "gr": [("Athens", ["ATH"])], "tr": [("Borsa Istanbul", ["IST"])],
    "ee": [("Nasdaq Tallinn", ["TAL"])], "lv": [("Nasdaq Riga", ["RIS"])], "lt": [("Nasdaq Vilnius", ["LIT"])],
    "il": [("Tel Aviv (TASE)", ["TLV"])], "sa": [("Saudi Exchange (Tadawul)", ["SAU"])],
    "qa": [("Qatar Stock Exchange", ["DOH"])], "kw": [("Boursa Kuwait", ["KUW"])],
    "eg": [("Egyptian Exchange", ["CAI"])], "za": [("Johannesburg (JSE)", ["JNB"])],
    "in": [("NSE", ["NSI"]), ("BSE", ["BSE"])],
    "cn": [("Shanghai", ["SHH"]), ("Shenzhen", ["SHZ"])], "hk": [("Hong Kong (HKEX)", ["HKG"])],
    "tw": [("Taiwan Stock Exchange", ["TAI"]), ("Taipei Exchange (TPEx)", ["TWO"])],
    "jp": [("Tokyo (JPX)", ["JPX"])], "kr": [("KOSPI", ["KSC"]), ("KOSDAQ", ["KOE"])],
    "sg": [("Singapore (SGX)", ["SES"])], "my": [("Bursa Malaysia", ["KLS"])], "th": [("Thailand (SET)", ["SET"])],
    "id": [("Indonesia (IDX)", ["JKT"])], "au": [("ASX", ["ASX"])], "nz": [("NZX", ["NZE"])],
}

SECTORS = ["Basic Materials", "Communication Services", "Consumer Cyclical", "Consumer Defensive", "Energy",
           "Financial Services", "Healthcare", "Industrials", "Real Estate", "Technology", "Utilities"]

# Industry names Yahoo actually matches (harvested from screener records and each verified to return
# rows). yfinance's list strips "&" and "-", which makes 56 of its entries match nothing.
INDUSTRIES = {
    "Basic Materials": ["Agricultural Inputs", "Aluminum", "Building Materials", "Chemicals", "Coking Coal",
                        "Copper", "Gold", "Lumber & Wood Production", "Other Industrial Metals & Mining",
                        "Other Precious Metals & Mining", "Paper & Paper Products", "Silver",
                        "Specialty Chemicals", "Steel"],
    "Communication Services": ["Advertising Agencies", "Broadcasting", "Electronic Gaming & Multimedia",
                               "Entertainment", "Internet Content & Information", "Publishing",
                               "Telecom Services"],
    "Consumer Cyclical": ["Apparel Manufacturing", "Apparel Retail", "Auto & Truck Dealerships",
                          "Auto Manufacturers", "Auto Parts", "Department Stores", "Footwear & Accessories",
                          "Furnishings, Fixtures & Appliances", "Gambling", "Home Improvement Retail",
                          "Internet Retail", "Leisure", "Lodging", "Luxury Goods", "Packaging & Containers",
                          "Personal Services", "Recreational Vehicles", "Residential Construction",
                          "Resorts & Casinos", "Restaurants", "Specialty Retail", "Textile Manufacturing",
                          "Travel Services"],
    "Consumer Defensive": ["Confectioners", "Discount Stores", "Education & Training Services", "Farm Products",
                           "Food Distribution", "Grocery Stores", "Household & Personal Products",
                           "Packaged Foods", "Tobacco"],
    "Energy": ["Oil & Gas Drilling", "Oil & Gas E&P", "Oil & Gas Equipment & Services", "Oil & Gas Integrated",
               "Oil & Gas Midstream", "Oil & Gas Refining & Marketing", "Thermal Coal", "Uranium"],
    "Financial Services": ["Asset Management", "Capital Markets", "Credit Services", "Financial Conglomerates",
                           "Financial Data & Stock Exchanges", "Insurance Brokers", "Mortgage Finance",
                           "Shell Companies"],
    "Healthcare": ["Biotechnology", "Diagnostics & Research", "Health Information Services", "Healthcare Plans",
                   "Medical Care Facilities", "Medical Devices", "Medical Distribution",
                   "Medical Instruments & Supplies", "Pharmaceutical Retailers"],
    "Industrials": ["Aerospace & Defense", "Airlines", "Airports & Air Services", "Building Products & Equipment",
                    "Business Equipment & Supplies", "Conglomerates", "Consulting Services",
                    "Electrical Equipment & Parts", "Engineering & Construction",
                    "Farm & Heavy Construction Machinery", "Industrial Distribution", "Infrastructure Operations",
                    "Integrated Freight & Logistics", "Marine Shipping", "Metal Fabrication",
                    "Pollution & Treatment Controls", "Railroads", "Rental & Leasing Services",
                    "Security & Protection Services", "Specialty Business Services",
                    "Specialty Industrial Machinery", "Staffing & Employment Services", "Tools & Accessories",
                    "Trucking", "Waste Management"],
    "Real Estate": ["Real Estate Services"],
    "Technology": ["Communication Equipment", "Computer Hardware", "Consumer Electronics", "Electronic Components",
                   "Electronics & Computer Distribution", "Information Technology Services",
                   "Scientific & Technical Instruments", "Semiconductor - Broad Line",
                   "Semiconductor Equipment & Materials", "Semiconductors", "Solar"],
    "Utilities": [],
}
ALL_INDUSTRIES = {name for names in INDUSTRIES.values() for name in names}

# Yahoo stores these industries with an em dash ("Banks—Regional") and its screener matches none
# of them under any spelling (verified). Steer the user to the peer group instead of returning 0 rows.
UNFILTERABLE_INDUSTRIES = {
    "Banks—Diversified": "Banks", "Banks—Regional": "Banks",
    "Insurance—Diversified": "Insurance", "Insurance—Life": "Insurance",
    "Insurance—Property & Casualty": "Insurance", "Insurance—Reinsurance": "Insurance",
    "Insurance—Specialty": "Insurance",
    "Beverages—Brewers": "Food Products", "Beverages—Non-Alcoholic": "Food Products",
    "Beverages—Wineries & Distilleries": "Food Products",
    "Drug Manufacturers—General": "Pharmaceuticals",
    "Drug Manufacturers—Specialty & Generic": "Pharmaceuticals",
    "REIT—Diversified": "Real Estate", "REIT—Healthcare Facilities": "Real Estate",
    "REIT—Hotel & Motel": "Real Estate", "REIT—Industrial": "Real Estate",
    "REIT—Mortgage": "Real Estate", "REIT—Office": "Real Estate", "REIT—Residential": "Real Estate",
    "REIT—Retail": "Real Estate", "REIT—Specialty": "Real Estate",
    "Real Estate—Development": "Real Estate", "Real Estate—Diversified": "Real Estate",
    "Software—Application": "Software & Services", "Software—Infrastructure": "Software & Services",
    "Utilities—Diversified": "Utilities", "Utilities—Independent Power Producers": "Utilities",
    "Utilities—Regulated Electric": "Utilities", "Utilities—Regulated Gas": "Utilities",
    "Utilities—Regulated Water": "Utilities", "Utilities—Renewable": "Utilities",
}

# Equity peer groups (the fund categories in yfinance's peer_group list are left out).
PEER_GROUPS = ["Aerospace & Defense", "Auto Components", "Automobiles", "Banks", "Building Products",
               "Chemicals", "Commercial Services", "Construction & Engineering", "Construction Materials",
               "Consumer Durables", "Consumer Services", "Containers & Packaging", "Diversified Financials",
               "Diversified Metals", "Electrical Equipment", "Energy Services", "Food Products", "Food Retailers",
               "Healthcare", "Homebuilders", "Household Products", "Industrial Conglomerates", "Insurance",
               "Machinery", "Media", "Oil & Gas Producers", "Paper & Forestry", "Pharmaceuticals",
               "Precious Metals", "Real Estate", "Refiners & Pipelines", "Retailing", "Semiconductors",
               "Software & Services", "Steel", "Technology Hardware", "Telecommunication Services",
               "Textiles & Apparel", "Traders & Distributors", "Transportation", "Transportation Infrastructure",
               "Utilities"]

# Yahoo's saved equity screens that return quote rows (probed). Titles are ours (Yahoo reuses
# "Day Gainers" for every region); Yahoo's own description is returned with each run.
PREDEFINED = [
    ("day_gainers", "Day Gainers", "US movers",
     "US stocks up more than 3% today; market cap >= $2B, price >= $5."),
    ("day_losers", "Day Losers", "US movers",
     "US stocks down more than 2.5% today; market cap >= $2B, price >= $5."),
    ("most_actives", "Most Actives", "US movers", "Highest-volume US stocks today; market cap >= $2B."),
    ("fifty_two_wk_gainers", "52-Week Gainers", "US movers", "Largest 52-week price gains."),
    ("fifty_two_wk_losers", "52-Week Losers", "US movers", "Largest 52-week price losses."),
    ("most_watched_tickers", "Most Watched", "US movers", "Most added to Yahoo Finance watchlists."),
    ("undervalued_growth_stocks", "Undervalued Growth", "Strategies",
     "EPS growth >= 25%, P/E 0-20 and PEG < 1 (NASDAQ / NYSE)."),
    ("undervalued_large_caps", "Undervalued Large Caps", "Strategies",
     "P/E 0-20 and PEG < 1 with market cap $10B-$100B (NASDAQ / NYSE)."),
    ("growth_technology_stocks", "Growth Technology", "Strategies",
     "Technology stocks with quarterly revenue growth and EPS growth >= 25%."),
    ("aggressive_small_caps", "Aggressive Small Caps", "Strategies", "Small caps with high earnings growth."),
    ("small_cap_gainers", "Small-Cap Gainers", "Strategies", "Small caps up more than 5% today."),
    ("high_dividend_yield", "High Dividend Yield", "Strategies", "Highest forward dividend yields."),
    # undervalued_wide_moat_stocks / strong_undervalued_stocks are Yahoo Premium screens: without a
    # subscription Yahoo returns only the first 5 rows, so they are not offered.
    ("most_shorted_stocks", "Most Shorted", "Strategies",
     "Highest short interest as % of shares outstanding (exchange reports, twice monthly)."),
    ("day_gainers_americas", "Day Gainers - Americas", "Regional movers", "Largest % gainers in the Americas."),
    ("day_losers_americas", "Day Losers - Americas", "Regional movers", "Largest % losers in the Americas."),
    ("most_actives_americas", "Most Actives - Americas", "Regional movers", "Highest volume in the Americas."),
    ("day_gainers_europe", "Day Gainers - Europe", "Regional movers", "Largest % gainers in Europe."),
    ("day_losers_europe", "Day Losers - Europe", "Regional movers", "Largest % losers in Europe."),
    ("most_actives_europe", "Most Actives - Europe", "Regional movers", "Highest volume in Europe."),
    ("day_gainers_asia", "Day Gainers - Asia", "Regional movers", "Largest % gainers in Asia."),
    ("day_losers_asia", "Day Losers - Asia", "Regional movers", "Largest % losers in Asia."),
    ("most_actives_asia", "Most Actives - Asia", "Regional movers", "Highest volume in Asia."),
]
PREDEFINED_BY_ID = {p[0]: p for p in PREDEFINED}

# Quote currencies expressed in a minor unit -> the major unit Yahoo uses for `marketCap`
# (verified: HSBA.L GBp -> GBP, BHG.JO ZAc -> ZAR, CYBR.TA ILA -> ILS, KFH.KW KWF -> KWD).
MINOR_TO_MAJOR = {"GBp": "GBP", "GBX": "GBP", "ZAc": "ZAR", "ZAC": "ZAR", "ILA": "ILS", "KWF": "KWD"}

CRITERIA_NOTE = ("Criteria are evaluated by Yahoo on its screener snapshot; price, change %, volume, "
                 "market cap and P/E shown are the current (exchange-delayed) quote, so a row can sit "
                 "slightly outside a price or % change bound.")


class ScreenerError(Exception):
    """A failure with a message fit to show the user."""


def _log(msg):
    print(f"[screener_data] {msg}", file=sys.stderr)


def _num(v):
    """Finite float or None (bool is not a number here)."""
    if isinstance(v, bool) or v is None:
        return None
    if isinstance(v, (int, float)):
        f = float(v)
        return f if math.isfinite(f) else None
    return None


def _int_arg(text, name, lo, hi):
    try:
        v = int(str(text).strip())
    except (TypeError, ValueError):
        raise ScreenerError(f"{name} must be an integer, got '{text}'")
    if v < lo or v > hi:
        raise ScreenerError(f"{name} must be between {lo} and {hi}, got {v}")
    return v


# ── Yahoo transport ──────────────────────────────────────────────────────────

def _session():
    # Lazy: the `fields` command must not pay for importing yfinance/pandas.
    from yfinance.data import YfData
    return YfData()


def _unwrap(resp, what):
    try:
        payload = resp.json()
    except Exception:
        raise ScreenerError(f"Yahoo {what} returned HTTP {resp.status_code} with a non-JSON body")
    finance = (payload or {}).get("finance") or {}
    err = finance.get("error")
    if err:
        desc = err.get("description") or err.get("code") or "unknown error"
        raise ScreenerError(f"Yahoo rejected the {what}: {desc}")
    if resp.status_code != 200:
        raise ScreenerError(f"Yahoo {what} failed with HTTP {resp.status_code}")
    result = finance.get("result") or []
    if not result or not isinstance(result[0], dict):
        raise ScreenerError(f"Yahoo {what} returned no result block")
    return result[0]


_BASE_PARAMS = {"formatted": "false", "lang": "en-US", "region": "US", "corsDomain": "finance.yahoo.com"}


def _post(session, body, records, what):
    params = dict(_BASE_PARAMS)
    if records:
        params["useRecordsResponse"] = "true"
    try:
        resp = session.post(SCREENER_URL, body=body, params=params, timeout=NET_TIMEOUT)
    except Exception as exc:  # curl_cffi / DNS / TLS / timeout
        raise ScreenerError(f"Network error contacting Yahoo Finance ({what}): {exc}")
    return _unwrap(resp, what)


def _get_predefined(session, screen_id, count):
    params = dict(_BASE_PARAMS, scrIds=screen_id, count=count)
    try:
        resp = session.get(url=PREDEFINED_URL, params=params, timeout=NET_TIMEOUT)
    except Exception as exc:
        raise ScreenerError(f"Network error contacting Yahoo Finance: {exc}")
    if resp.status_code == 404:
        raise ScreenerError(f"Yahoo has no predefined screen '{screen_id}'")
    return _unwrap(resp, "predefined screen")


# ── Query building ───────────────────────────────────────────────────────────

def _eq_or(field, values):
    clauses = [{"operator": "EQ", "operands": [field, v]} for v in values]
    return clauses[0] if len(clauses) == 1 else {"operator": "OR", "operands": clauses}


def _as_list(value):
    if value is None or value == "":
        return []
    if isinstance(value, (list, tuple)):
        return [v for v in value if v not in (None, "")]
    return [value]


def _resolve_field(name):
    """Friendly id or raw Yahoo field -> catalogue tuple."""
    key = str(name or "").strip()
    f = FIELD_BY_ID.get(key) or FIELD_BY_YAHOO.get(key)
    if not f:
        raise ScreenerError(f"Unknown screener field '{key}'. Run `screener_data.py fields` for the list.")
    return f


def _numeric_clause(flt, index):
    if not isinstance(flt, dict):
        raise ScreenerError(f"Filter {index} must be an object with field, op and value")
    f = _resolve_field(flt.get("field"))
    op = _OP_ALIASES.get(str(flt.get("op", "")).strip().lower())
    if not op or op == "EQ":
        raise ScreenerError(f"Filter {index} ({f[1]}): operator must be one of gt, gte, lt, lte, btwn")
    value = flt.get("value")
    if op == "BTWN":
        if not isinstance(value, (list, tuple)) or len(value) != 2:
            raise ScreenerError(f"Filter {index} ({f[1]}): 'between' needs two values [min, max]")
        lo, hi = _num(value[0]), _num(value[1])
        if lo is None or hi is None:
            raise ScreenerError(f"Filter {index} ({f[1]}): 'between' values must be numbers")
        if lo > hi:
            lo, hi = hi, lo
        return f, {"operator": "BTWN", "operands": [f[2], lo, hi]}
    v = _num(value)
    if v is None:
        raise ScreenerError(f"Filter {index} ({f[1]}): value must be a number")
    return f, {"operator": op, "operands": [f[2], v]}


def build_custom_query(criteria):
    """criteria dict -> (yahoo query, [field ids referenced by numeric filters, in order])."""
    if not isinstance(criteria, dict):
        raise ScreenerError("Criteria must be a JSON object")
    clauses = []

    regions = [str(r).strip().lower() for r in _as_list(criteria.get("region"))]
    valid_regions = {code for code, _ in REGIONS}
    bad = [r for r in regions if r not in valid_regions]
    if bad:
        raise ScreenerError(f"Unknown or empty region code(s): {', '.join(bad)}")
    if regions:
        clauses.append(_eq_or("region", regions))

    exchanges = [str(x).strip().upper() for x in _as_list(criteria.get("exchange"))]
    valid_exchanges = {c for groups in EXCHANGES.values() for _, codes in groups for c in codes}
    bad = [x for x in exchanges if x not in valid_exchanges]
    if bad:
        raise ScreenerError(f"Unknown or empty exchange code(s): {', '.join(bad)}")
    if exchanges:
        clauses.append(_eq_or("exchange", list(dict.fromkeys(exchanges))))

    sectors = [str(s).strip() for s in _as_list(criteria.get("sector"))]
    bad = [s for s in sectors if s not in SECTORS]
    if bad:
        raise ScreenerError(f"Unknown sector(s): {', '.join(bad)}")
    if sectors:
        clauses.append(_eq_or("sector", sectors))

    industries = [str(i).strip() for i in _as_list(criteria.get("industry"))]
    for ind in industries:
        if ind in UNFILTERABLE_INDUSTRIES:
            raise ScreenerError(f"Yahoo cannot filter the industry '{ind}'. Use peer group "
                                f"'{UNFILTERABLE_INDUSTRIES[ind]}' (or sector) instead.")
        if ind not in ALL_INDUSTRIES:
            raise ScreenerError(f"Unknown industry '{ind}'")
    if industries:
        clauses.append(_eq_or("industry", industries))

    peers = [str(p).strip() for p in _as_list(criteria.get("peer_group"))]
    bad = [p for p in peers if p not in PEER_GROUPS]
    if bad:
        raise ScreenerError(f"Unknown peer group(s): {', '.join(bad)}")
    if peers:
        clauses.append(_eq_or("peer_group", peers))

    used = []
    filters = criteria.get("filters") or []
    if not isinstance(filters, list):
        raise ScreenerError("'filters' must be a list")
    for i, flt in enumerate(filters, start=1):
        f, clause = _numeric_clause(flt, i)
        clauses.append(clause)
        if f[0] not in used:
            used.append(f[0])

    if not clauses:
        raise ScreenerError("Add at least one criterion (a region, sector or numeric filter) before running.")
    query = clauses[0] if len(clauses) == 1 else {"operator": "AND", "operands": clauses}
    return query, used


def _describe(node):
    """Compact human text for a Yahoo query tree (shown as the screen's criteria)."""
    if not isinstance(node, dict):
        return str(node)
    op = str(node.get("operator", "")).upper()
    ops = node.get("operands") or []
    if op in ("AND", "OR"):
        parts = [_describe(o) for o in ops]
        joined = (" AND " if op == "AND" else " OR ").join(parts)
        return joined if op == "AND" else f"({joined})"
    if op == "MUST_NOT":
        return "NOT " + _describe({"operator": "OR", "operands": ops})
    if not ops:
        return op
    field = str(ops[0])
    label = FIELD_BY_YAHOO[field][1] if field in FIELD_BY_YAHOO else field
    vals = ops[1:]
    sym = {"GT": ">", "GTE": ">=", "LT": "<", "LTE": "<=", "EQ": "="}.get(op)
    if op == "BTWN" and len(vals) == 2:
        return f"{label} {_fmt_val(vals[0])} to {_fmt_val(vals[1])}"
    if sym and vals:
        return f"{label} {sym} {_fmt_val(vals[0])}"
    return f"{label} {op.lower()} {', '.join(_fmt_val(v) for v in vals)}"


def _fmt_val(v):
    n = _num(v)
    if n is None:
        return str(v)
    a = abs(n)
    for div, suffix in ((1e12, "T"), (1e9, "B"), (1e6, "M")):
        if a >= div:
            return f"{n / div:g}{suffix}"
    return f"{n:g}"


# ── Result assembly ──────────────────────────────────────────────────────────

def _row_from_quote(q):
    cur = q.get("currency")
    return {
        "symbol": q.get("symbol"),
        "name": q.get("longName") or q.get("shortName") or q.get("displayName") or q.get("symbol"),
        "price": _num(q.get("regularMarketPrice")),
        "change": _num(q.get("regularMarketChange")),
        "change_pct": _num(q.get("regularMarketChangePercent")),
        "volume": _num(q.get("regularMarketVolume")),
        "avg_volume_3m": _num(q.get("averageDailyVolume3Month")),
        "market_cap": _num(q.get("marketCap")),
        "pe": _num(q.get("trailingPE")),
        "forward_pe": _num(q.get("forwardPE")),
        "exchange": q.get("exchange"),
        "exchange_name": q.get("fullExchangeName") or q.get("exchange"),
        "currency": cur,
        "market_cap_currency": MINOR_TO_MAJOR.get(cur, cur),
        "financial_currency": q.get("financialCurrency"),
        "market_time": q.get("regularMarketTime"),
        "delay_min": q.get("exchangeDataDelayedBy"),
        "sector": None,
        "industry": None,
        "beta": None,
        "fields": {},
    }


def _records_for(session, symbols, columns):
    """Ticker -> records-format row (sector, industry, beta + the columns' fields), one Yahoo call."""
    if not symbols:
        return {}
    include = ["ticker", "sector", "industry", "beta", "peratio.lasttwelvemonths"]
    for col in columns:
        yahoo = FIELD_BY_ID[col["field"]][2]
        if yahoo not in include:
            include.append(yahoo)
    body = {
        "offset": 0, "size": len(symbols), "sortField": "ticker", "sortType": "ASC", "quoteType": "EQUITY",
        "query": _eq_or("ticker", symbols), "userId": "", "userIdType": "guid", "includeFields": include,
    }
    res = _post(session, body, records=True, what="sector/fundamentals lookup")
    return {r.get("ticker"): r for r in (res.get("records") or []) if isinstance(r, dict)}


def _assemble(session, kind, quotes, total, offset, columns, sort_field, sort_asc, meta):
    rows = [_row_from_quote(q) for q in quotes if isinstance(q, dict) and q.get("symbol")]
    warnings = list(meta.pop("warnings", []))
    symbols = [r["symbol"] for r in rows]
    try:
        records = _records_for(session, symbols, columns)
    except ScreenerError as exc:
        _log(f"records lookup failed: {exc}")
        records = {}
        warnings.append(f"Sector, industry and criteria columns are unavailable for this run: {exc}")

    for row in rows:
        rec = records.get(row["symbol"]) or {}
        row["sector"] = rec.get("sector")
        row["industry"] = rec.get("industry")
        row["beta"] = _num(rec.get("beta"))
        if row["pe"] is None:  # the quote omits trailingPE for some listings; fall back to the snapshot's
            row["pe"] = _num(rec.get("peRatioLtm"))
        for col in columns:
            row["fields"][col["id"]] = _num(rec.get(FIELD_BY_ID[col["field"]][3]))
    if records and len(records) < len(rows):
        warnings.append(f"Yahoo returned fundamentals for {len(records)} of {len(rows)} rows.")

    times = [int(r["market_time"]) for r in rows if isinstance(r.get("market_time"), (int, float))]
    delays = [int(r["delay_min"]) for r in rows if isinstance(r.get("delay_min"), (int, float))]
    out = {
        "source": SOURCE,
        "kind": kind,
        "total": int(total) if isinstance(total, (int, float)) else len(rows),
        "offset": offset,
        "count": len(rows),
        "sort_field": sort_field,
        "sort_asc": bool(sort_asc),
        "columns": [{k: c[k] for k in ("id", "field", "label", "unit", "description")} for c in columns],
        "rows": rows,
        "as_of": max(times) if times else None,
        "max_delay_min": max(delays) if delays else None,
        "fetched_at": int(time.time()),
        "criteria_note": CRITERIA_NOTE,
        "warnings": warnings,
    }
    out.update(meta)
    return out


def _extra_columns(field_ids, sort_field_id):
    """Columns beyond the always-shown base set: every criteria field, then the sort field.

    Base columns show the live quote, but Yahoo filtered and ranked on its lagging snapshot. For the
    fast-moving base fields (% change, volume, price) that drive the screen — the sort key, or a
    % change criterion — the snapshot value gets its own "@ screen" column, which is what explains
    why a row is in the list and where it ranks (e.g. Day Gainers sorts on snapshot % change).
    """
    cols = []
    seen = set()
    for fid in list(field_ids) + ([sort_field_id] if sort_field_id else []):
        if fid not in FIELD_BY_ID or fid in seen or fid in BASE_COLUMNS:
            continue
        seen.add(fid)
        f = FIELD_BY_ID[fid]
        cols.append({"id": fid, "field": fid, "label": f[1], "unit": f[5], "description": f[6]})
    snapshot = []
    if sort_field_id in ("change_pct", "volume", "price"):
        snapshot.append(sort_field_id)
    if "change_pct" in field_ids and "change_pct" not in snapshot:
        snapshot.append("change_pct")
    for fid in snapshot:
        f = FIELD_BY_ID[fid]
        cols.append({"id": fid + "_screen", "field": fid, "label": f[1] + " @ screen", "unit": f[5],
                     "description": (f"{f[1]} in Yahoo's screener snapshot, which Yahoo filtered and ranked on. "
                                     f"The {f[1]} column shows the current quote.")})
    return cols


def run_custom(criteria, count, sort_field, sort_asc, offset):
    query, used = build_custom_query(criteria)
    sort_f = _resolve_field(sort_field or DEFAULT_SORT)
    session = _session()
    body = {
        "offset": offset, "size": count, "sortField": sort_f[2], "sortType": "ASC" if sort_asc else "DESC",
        "quoteType": "EQUITY", "query": query, "userId": "", "userIdType": "guid",
    }
    _log(f"custom screen: {json.dumps(query)[:400]} sort={sort_f[2]} asc={sort_asc} count={count} offset={offset}")
    res = _post(session, body, records=False, what="screen")
    meta = {"screen_id": None, "title": "Custom screen", "description": "", "criteria_text": _describe(query)}
    return _assemble(session, "custom", res.get("quotes") or [], res.get("total"), offset,
                     _extra_columns(used, sort_f[0]), sort_f[0], sort_asc, meta)


def run_predefined(screen_id, count, offset):
    session = _session()
    _log(f"predefined screen {screen_id} count={count} offset={offset}")
    # The predefined endpoint ignores offset, so a later page replays the screen's own criteria
    # (rawCriteria) against the custom endpoint — the same thing yfinance does.
    res = _get_predefined(session, screen_id, count if offset == 0 else 1)
    meta_block = res.get("criteriaMeta") or {}
    raw = res.get("rawCriteria")
    query = None
    if raw:
        try:
            query = (json.loads(raw) or {}).get("query")
        except (TypeError, ValueError):
            query = None
    sort_yahoo = meta_block.get("sortField")
    sort_asc = str(meta_block.get("sortType") or "DESC").upper() == "ASC"
    quotes = res.get("quotes") or []
    total = res.get("total")
    if offset > 0:
        if not query:
            raise ScreenerError("Yahoo does not expose this screen's criteria, so it cannot be paged")
        body = {"offset": offset, "size": count, "sortField": sort_yahoo or "ticker",
                "sortType": "ASC" if sort_asc else "DESC", "quoteType": meta_block.get("quoteType") or "EQUITY",
                "query": query, "userId": "", "userIdType": "guid"}
        page = _post(session, body, records=False, what="screen page")
        quotes = page.get("quotes") or []
        total = page.get("total", total)

    crit_fields = [FIELD_BY_YAHOO[c.get("field")][0] for c in (meta_block.get("criteria") or [])
                   if isinstance(c, dict) and c.get("field") in FIELD_BY_YAHOO]
    sort_id = FIELD_BY_YAHOO[sort_yahoo][0] if sort_yahoo in FIELD_BY_YAHOO else sort_yahoo
    local = PREDEFINED_BY_ID.get(screen_id)
    warnings = []
    if res.get("isPremium") and len(quotes) < count and isinstance(total, int) and total > len(quotes):
        warnings.append(f"This is a Yahoo Premium screen: Yahoo returned only {len(quotes)} of {total} matches.")
    meta = {
        "screen_id": screen_id,
        "title": local[1] if local else (res.get("title") or screen_id),
        "description": res.get("description") or (local[3] if local else ""),
        "criteria_text": _describe(query) if query else "",
        "warnings": warnings,
    }
    return _assemble(session, "predefined", quotes, total, offset, _extra_columns(crit_fields, sort_id), sort_id,
                     sort_asc, meta)


def cmd_fields():
    def unit_label(unit):
        return {"pct": "%", "ratio": "x", "price": "quote ccy", "mcap": "quote ccy", "money": "reporting ccy",
                "per_share": "reporting ccy / share", "shares": "shares", "days": "days", "years": "years",
                "score": ""}.get(unit, "")

    return {
        "source": SOURCE,
        "operators": OPERATORS,
        "categories": CATEGORIES,
        "fields": [{"id": f[0], "label": f[1], "yahoo": f[2], "category": f[4], "unit": f[5],
                    "unit_label": unit_label(f[5]), "description": f[6]} for f in FIELDS],
        "base_columns": list(BASE_COLUMNS),
        "default_sort": DEFAULT_SORT,
        "default_count": DEFAULT_COUNT,
        "max_count": MAX_COUNT,
        "regions": [{"code": c, "name": n} for c, n in REGIONS],
        "exchanges": {reg: [{"label": label, "codes": codes} for label, codes in groups]
                      for reg, groups in EXCHANGES.items()},
        "sectors": SECTORS,
        "industries": INDUSTRIES,
        "unfilterable_industries": UNFILTERABLE_INDUSTRIES,
        "peer_groups": PEER_GROUPS,
        "predefined": [{"id": p[0], "title": p[1], "group": p[2], "description": p[3]} for p in PREDEFINED],
        "criteria_note": CRITERIA_NOTE,
    }


def _read_criteria(text):
    if text == "-":
        text = sys.stdin.read()
    try:
        criteria = json.loads(text)
    except (TypeError, ValueError) as exc:
        raise ScreenerError(f"Criteria are not valid JSON: {exc}")
    if not isinstance(criteria, dict):
        raise ScreenerError("Criteria must be a JSON object")
    return criteria


def _arg(argv, i):
    """Positional argument i, or None when absent/blank (callers pass "" to skip a slot)."""
    return argv[i].strip() if len(argv) > i and argv[i] and argv[i].strip() else None


def main(argv):
    if not argv:
        raise ScreenerError("Usage: screener_data.py predefined <id> [count] [offset] | "
                            "custom <criteria-json|-> [count] [sort_field] [asc|desc] [offset] | fields")
    cmd = argv[0]
    if cmd == "fields":
        return cmd_fields()
    if cmd == "predefined":
        screen_id = _arg(argv, 1)
        if not screen_id:
            raise ScreenerError("Usage: screener_data.py predefined <screen_id> [count] [offset]")
        count = _int_arg(_arg(argv, 2) or DEFAULT_COUNT, "count", 1, MAX_COUNT)
        offset = _int_arg(_arg(argv, 3) or 0, "offset", 0, 100_000)
        return run_predefined(screen_id, count, offset)
    if cmd == "custom":
        if _arg(argv, 1) is None:
            raise ScreenerError("Usage: screener_data.py custom <criteria-json|-> [count] [sort_field] "
                                "[asc|desc] [offset]")
        criteria = _read_criteria(_arg(argv, 1))
        count = _int_arg(_arg(argv, 2) or criteria.get("count", DEFAULT_COUNT), "count", 1, MAX_COUNT)
        sort_field = _arg(argv, 3) or criteria.get("sort_field")
        direction = _arg(argv, 4)
        if direction is not None:
            if direction.lower() not in ("asc", "desc"):
                raise ScreenerError(f"Sort direction must be asc or desc, got '{direction}'")
            sort_asc = direction.lower() == "asc"
        else:
            sort_asc = bool(criteria.get("sort_asc", False))
        offset = _int_arg(_arg(argv, 5) or criteria.get("offset", 0), "offset", 0, 100_000)
        return run_custom(criteria, count, sort_field, sort_asc, offset)
    raise ScreenerError(f"Unknown command '{cmd}'. Use predefined, custom or fields.")


def _emit(obj):
    # ASCII-escaped: company and industry names carry non-ASCII (e.g. em dashes) and a console
    # stdout on Windows is not UTF-8; the C++ side decodes \\u escapes either way.
    print(fincept_json.dumps(obj, ensure_ascii=True))


if __name__ == "__main__":
    try:
        _emit(main(sys.argv[1:]))
        sys.exit(0)
    except ScreenerError as exc:
        _emit({"error": str(exc)})
        sys.exit(1)
    except Exception as exc:  # never let a traceback replace the JSON contract
        _log(f"unexpected failure: {type(exc).__name__}: {exc}")
        _emit({"error": f"Screener failed: {type(exc).__name__}: {exc}"})
        sys.exit(1)
