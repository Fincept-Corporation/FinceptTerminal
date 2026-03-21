#!/usr/bin/env python3
"""
Bank of England (BoE) Statistical Interactive Database (IADB) Wrapper

Provides programmatic access to the Bank of England's IADB covering:
- Official Bank Rate (base rate)
- SONIA (Sterling Overnight Index Average)
- Spot exchange rates (GBP vs 26+ currencies)
- Sterling Exchange Rate Index (ERI / trade-weighted)
- Gilt yield curves (nominal, real, inflation, OIS)
- Quoted household interest rates (mortgages, deposits, consumer credit)
- Effective interest rates
- Monetary aggregates (M0, M4, M4Lx, notes & coin)
- Balance sheet / Bankstats data

API details:
- Base URL: https://www.bankofengland.co.uk/boeapps/database/_iadb-fromshowcolumns.asp
- No authentication required (completely free and open)
- Response formats: CSV (primary), XML, HTML, Excel
- Date format: DD/Mon/YYYY (e.g. 01/Jan/2000)
- Up to 300 series codes per CSV/XML/HTML request; 250 for Excel
- Missing values represented as ".." (two dots) -> converted to None/NaN

Usage:
    python boe_data.py available_categories
    python boe_data.py bank_rate [--start 01/Jan/2000] [--end now]
    python boe_data.py sonia [--start 01/Jan/2020] [--end now]
    python boe_data.py exchange_rates [--start 01/Jan/2020] [--end now]
    python boe_data.py exchange_rate_index [--start 01/Jan/2020] [--end now]
    python boe_data.py quoted_rates [--start 01/Jan/2012] [--end now]
    python boe_data.py effective_rates [--start 01/Jan/2012] [--end now]
    python boe_data.py monetary_aggregates [--start 01/Jan/2010] [--end now]
    python boe_data.py series --codes IUDBEDR,IUDSOIA [--start 01/Jan/2020] [--end now]
    python boe_data.py overview
"""

import sys
import json
import io
import csv
import requests
from datetime import datetime, date
from typing import Dict, List, Optional, Any, Union

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

BOE_BASE_URL = "https://www.bankofengland.co.uk/boeapps/database/_iadb-fromshowcolumns.asp"

# Reusable session with retry behaviour
_session = requests.Session()
_adapter = requests.adapters.HTTPAdapter(
    pool_connections=5,
    pool_maxsize=10,
    max_retries=requests.adapters.Retry(total=3, backoff_factor=1),
)
_session.mount("https://", _adapter)
_session.headers.update({
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/120.0.0.0 Safari/537.36"
    )
})

# ---------------------------------------------------------------------------
# Key series-code reference catalogue
# ---------------------------------------------------------------------------

SERIES_CATALOGUE: Dict[str, Dict[str, Any]] = {
    # --- Monetary Policy -------------------------------------------------------
    "IUDBEDR": {
        "name": "Official Bank Rate (Base Rate)",
        "category": "interest_rates",
        "subcategory": "monetary_policy",
        "frequency": "daily",
        "description": "Bank of England policy rate set by the Monetary Policy Committee (MPC).",
        "unit": "percent",
    },
    "IUDSOIA": {
        "name": "Daily SONIA Rate",
        "category": "interest_rates",
        "subcategory": "money_market",
        "frequency": "daily",
        "description": "Sterling Overnight Index Average – effective overnight interbank rate.",
        "unit": "percent",
    },
    "IUASOIA": {
        "name": "SONIA Compounded Index",
        "category": "interest_rates",
        "subcategory": "money_market",
        "frequency": "daily",
        "description": "SONIA Compounded Index published from 3 August 2020 for RFR transition.",
        "unit": "index",
    },

    # --- Government Gilt Yield Curves (nominal par yields) ---------------------
    "IUDSNPY": {
        "name": "Nominal Par Yield – 5Y",
        "category": "yield_curves",
        "subcategory": "gilt_nominal",
        "frequency": "daily",
        "description": "UK gilt nominal par yield, 5-year maturity.",
        "unit": "percent",
    },
    "IUDMNPY": {
        "name": "Nominal Par Yield – 10Y",
        "category": "yield_curves",
        "subcategory": "gilt_nominal",
        "frequency": "daily",
        "description": "UK gilt nominal par yield, 10-year maturity.",
        "unit": "percent",
    },
    "IUDLNPY": {
        "name": "Nominal Par Yield – 20Y",
        "category": "yield_curves",
        "subcategory": "gilt_nominal",
        "frequency": "daily",
        "description": "UK gilt nominal par yield, 20-year maturity.",
        "unit": "percent",
    },
    "IUDSIZC": {
        "name": "Implied Zero-Coupon Inflation Curve – 5Y",
        "category": "yield_curves",
        "subcategory": "gilt_inflation",
        "frequency": "daily",
        "description": "Implied zero-coupon inflation term structure, 5-year maturity.",
        "unit": "percent",
    },
    "IUDMIZC": {
        "name": "Implied Zero-Coupon Inflation Curve – 10Y",
        "category": "yield_curves",
        "subcategory": "gilt_inflation",
        "frequency": "daily",
        "description": "Implied zero-coupon inflation term structure, 10-year maturity.",
        "unit": "percent",
    },

    # --- OIS (Overnight Index Swap) Yield Curves ------------------------------
    "IUDZOS2": {
        "name": "OIS Zero-Coupon Spot Rate – 2Y",
        "category": "yield_curves",
        "subcategory": "ois",
        "frequency": "daily",
        "description": "SONIA-based OIS zero-coupon spot rate, 2-year maturity.",
        "unit": "percent",
    },
    "IUDZLT2": {
        "name": "OIS Zero-Coupon Spot Rate – 5Y",
        "category": "yield_curves",
        "subcategory": "ois",
        "frequency": "daily",
        "description": "SONIA-based OIS zero-coupon spot rate, 5-year maturity.",
        "unit": "percent",
    },
    "IUDZLS6": {
        "name": "OIS Zero-Coupon Spot Rate – 10Y",
        "category": "yield_curves",
        "subcategory": "ois",
        "frequency": "daily",
        "description": "SONIA-based OIS zero-coupon spot rate, 10-year maturity.",
        "unit": "percent",
    },
    "IUDZLS7": {
        "name": "OIS Zero-Coupon Spot Rate – 25Y",
        "category": "yield_curves",
        "subcategory": "ois",
        "frequency": "daily",
        "description": "SONIA-based OIS zero-coupon spot rate, 25-year maturity (extended in 2021).",
        "unit": "percent",
    },

    # --- GBP Spot Exchange Rates (XUDL prefix) --------------------------------
    "XUDLUSS": {
        "name": "GBP/USD Spot Rate",
        "category": "exchange_rates",
        "subcategory": "gbp_spot",
        "frequency": "daily",
        "description": "US dollar into sterling – indicative 4pm mid-market rate.",
        "unit": "GBP per 1 USD (inverted: USD per GBP in practice)",
    },
    "XUDLERS": {
        "name": "GBP/EUR Spot Rate",
        "category": "exchange_rates",
        "subcategory": "gbp_spot",
        "frequency": "daily",
        "description": "Euro into sterling – indicative 4pm mid-market rate.",
        "unit": "GBP per 1 EUR",
    },
    "XUDLJYS": {
        "name": "GBP/JPY Spot Rate",
        "category": "exchange_rates",
        "subcategory": "gbp_spot",
        "frequency": "daily",
        "description": "Japanese yen into sterling – indicative 4pm mid-market rate.",
        "unit": "GBP per 1 JPY",
    },
    "XUDLCDS": {
        "name": "GBP/CAD Spot Rate",
        "category": "exchange_rates",
        "subcategory": "gbp_spot",
        "frequency": "daily",
        "description": "Canadian dollar into sterling.",
        "unit": "GBP per 1 CAD",
    },
    "XUDLSFS": {
        "name": "GBP/CHF Spot Rate",
        "category": "exchange_rates",
        "subcategory": "gbp_spot",
        "frequency": "daily",
        "description": "Swiss franc into sterling.",
        "unit": "GBP per 1 CHF",
    },
    "XUDLADS": {
        "name": "GBP/AUD Spot Rate",
        "category": "exchange_rates",
        "subcategory": "gbp_spot",
        "frequency": "daily",
        "description": "Australian dollar into sterling.",
        "unit": "GBP per 1 AUD",
    },
    "XUDLNKS": {
        "name": "GBP/NOK Spot Rate",
        "category": "exchange_rates",
        "subcategory": "gbp_spot",
        "frequency": "daily",
        "description": "Norwegian krone into sterling.",
        "unit": "GBP per 1 NOK",
    },
    "XUDLSKS": {
        "name": "GBP/SEK Spot Rate",
        "category": "exchange_rates",
        "subcategory": "gbp_spot",
        "frequency": "daily",
        "description": "Swedish krona into sterling.",
        "unit": "GBP per 1 SEK",
    },
    "XUDLDKS": {
        "name": "GBP/DKK Spot Rate",
        "category": "exchange_rates",
        "subcategory": "gbp_spot",
        "frequency": "daily",
        "description": "Danish krone into sterling.",
        "unit": "GBP per 1 DKK",
    },
    "XUDLSGS": {
        "name": "GBP/SGD Spot Rate",
        "category": "exchange_rates",
        "subcategory": "gbp_spot",
        "frequency": "daily",
        "description": "Singapore dollar into sterling.",
        "unit": "GBP per 1 SGD",
    },
    "XUDLHDS": {
        "name": "GBP/HKD Spot Rate",
        "category": "exchange_rates",
        "subcategory": "gbp_spot",
        "frequency": "daily",
        "description": "Hong Kong dollar into sterling.",
        "unit": "GBP per 1 HKD",
    },
    "XUDLTWS": {
        "name": "GBP/TWD Spot Rate",
        "category": "exchange_rates",
        "subcategory": "gbp_spot",
        "frequency": "daily",
        "description": "New Taiwan dollar into sterling.",
        "unit": "GBP per 1 TWD",
    },
    "XUDLBK25": {
        "name": "GBP/MYR Spot Rate",
        "category": "exchange_rates",
        "subcategory": "gbp_spot",
        "frequency": "daily",
        "description": "Malaysian ringgit into sterling.",
        "unit": "GBP per 1 MYR",
    },
    "XUDLBK47": {
        "name": "GBP/HKD Spot Rate (alt)",
        "category": "exchange_rates",
        "subcategory": "gbp_spot",
        "frequency": "daily",
        "description": "Alternative Hong Kong dollar spot rate series.",
        "unit": "GBP per 1 HKD",
    },
    "XUDLBK83": {
        "name": "GBP/INR Spot Rate",
        "category": "exchange_rates",
        "subcategory": "gbp_spot",
        "frequency": "daily",
        "description": "Indian rupee into sterling.",
        "unit": "GBP per 1 INR",
    },
    "XUDLBK85": {
        "name": "GBP/KRW Spot Rate",
        "category": "exchange_rates",
        "subcategory": "gbp_spot",
        "frequency": "daily",
        "description": "South Korean won into sterling.",
        "unit": "GBP per 1 KRW",
    },
    "XUDLBK87": {
        "name": "GBP/ZAR Spot Rate",
        "category": "exchange_rates",
        "subcategory": "gbp_spot",
        "frequency": "daily",
        "description": "South African rand into sterling.",
        "unit": "GBP per 1 ZAR",
    },

    # --- USD Cross Rates (XUDL prefix) ----------------------------------------
    "XUDLADD": {
        "name": "USD/AUD Spot Rate",
        "category": "exchange_rates",
        "subcategory": "usd_cross",
        "frequency": "daily",
        "description": "Australian dollar into USD – BoE published cross rate.",
        "unit": "USD per 1 AUD",
    },
    "XUDLBFD": {
        "name": "USD/CHF Spot Rate",
        "category": "exchange_rates",
        "subcategory": "usd_cross",
        "frequency": "daily",
        "description": "Swiss franc into USD – BoE published cross rate.",
        "unit": "USD per 1 CHF",
    },

    # --- Sterling Exchange Rate Index (ERI) -----------------------------------
    "XUMAERS": {
        "name": "Sterling ERI (Narrow, Chain-Linked)",
        "category": "exchange_rates",
        "subcategory": "eri",
        "frequency": "daily",
        "description": (
            "Trade-weighted sterling exchange rate index (narrow basis). "
            "Jan 2005 = 100. Annually re-weighted using world trade data."
        ),
        "unit": "index (Jan 2005=100)",
    },
    "XUMABRD": {
        "name": "Sterling ERI (Broad, Chain-Linked)",
        "category": "exchange_rates",
        "subcategory": "eri",
        "frequency": "daily",
        "description": "Trade-weighted sterling exchange rate index (broad basis). Jan 2005 = 100.",
        "unit": "index (Jan 2005=100)",
    },

    # --- USD/EUR Forward Discount Rates ---------------------------------------
    "XUDLDF1": {
        "name": "USD/GBP 1-Month Forward Discount",
        "category": "exchange_rates",
        "subcategory": "forward",
        "frequency": "daily",
        "description": "USD into sterling forward premium/discount, 1-month.",
        "unit": "percent per annum",
    },
    "XUDLDF3": {
        "name": "USD/GBP 3-Month Forward Discount",
        "category": "exchange_rates",
        "subcategory": "forward",
        "frequency": "daily",
        "description": "USD into sterling forward premium/discount, 3-month.",
        "unit": "percent per annum",
    },
    "XUDLDF6": {
        "name": "USD/GBP 6-Month Forward Discount",
        "category": "exchange_rates",
        "subcategory": "forward",
        "frequency": "daily",
        "description": "USD into sterling forward premium/discount, 6-month.",
        "unit": "percent per annum",
    },
    "XUDLDFY": {
        "name": "USD/GBP 12-Month Forward Discount",
        "category": "exchange_rates",
        "subcategory": "forward",
        "frequency": "daily",
        "description": "USD into sterling forward premium/discount, 12-month.",
        "unit": "percent per annum",
    },

    # --- Quoted Household Interest Rates (IUM prefix) -------------------------
    "IUMZICQ": {
        "name": "Mortgage Rate – 2Y Fixed 60% LTV",
        "category": "interest_rates",
        "subcategory": "quoted_mortgage",
        "frequency": "monthly",
        "description": "2-year fixed-rate mortgage, 60% loan-to-value ratio.",
        "unit": "percent per annum",
    },
    "IUMBV34": {
        "name": "Mortgage Rate – 2Y Fixed 75% LTV",
        "category": "interest_rates",
        "subcategory": "quoted_mortgage",
        "frequency": "monthly",
        "description": "2-year fixed-rate mortgage, 75% LTV.",
        "unit": "percent per annum",
    },
    "IUMZICR": {
        "name": "Mortgage Rate – 2Y Fixed 85% LTV",
        "category": "interest_rates",
        "subcategory": "quoted_mortgage",
        "frequency": "monthly",
        "description": "2-year fixed-rate mortgage, 85% LTV.",
        "unit": "percent per annum",
    },
    "IUMB482": {
        "name": "Mortgage Rate – 2Y Fixed 90% LTV",
        "category": "interest_rates",
        "subcategory": "quoted_mortgage",
        "frequency": "monthly",
        "description": "2-year fixed-rate mortgage, 90% LTV.",
        "unit": "percent per annum",
    },
    "IUM2WTL": {
        "name": "Mortgage Rate – 2Y Fixed 95% LTV",
        "category": "interest_rates",
        "subcategory": "quoted_mortgage",
        "frequency": "monthly",
        "description": "2-year fixed-rate mortgage, 95% LTV.",
        "unit": "percent per annum",
    },
    "IUMBV37": {
        "name": "Mortgage Rate – 3Y Fixed",
        "category": "interest_rates",
        "subcategory": "quoted_mortgage",
        "frequency": "monthly",
        "description": "3-year fixed-rate mortgage.",
        "unit": "percent per annum",
    },
    "IUMBV42": {
        "name": "Mortgage Rate – 5Y Fixed 75% LTV",
        "category": "interest_rates",
        "subcategory": "quoted_mortgage",
        "frequency": "monthly",
        "description": "5-year fixed-rate mortgage, 75% LTV.",
        "unit": "percent per annum",
    },
    "IUM5WTL": {
        "name": "Mortgage Rate – 5Y Fixed 95% LTV",
        "category": "interest_rates",
        "subcategory": "quoted_mortgage",
        "frequency": "monthly",
        "description": "5-year fixed-rate mortgage, 95% LTV.",
        "unit": "percent per annum",
    },
    "IUMBV45": {
        "name": "Mortgage Rate – 10Y Fixed",
        "category": "interest_rates",
        "subcategory": "quoted_mortgage",
        "frequency": "monthly",
        "description": "10-year fixed-rate mortgage.",
        "unit": "percent per annum",
    },
    "IUMB479": {
        "name": "Mortgage Rate – Variable/Tracker",
        "category": "interest_rates",
        "subcategory": "quoted_mortgage",
        "frequency": "monthly",
        "description": "Variable or tracker mortgage rate.",
        "unit": "percent per annum",
    },
    "IUMBV24": {
        "name": "Mortgage Rate – Standard Variable Rate (SVR)",
        "category": "interest_rates",
        "subcategory": "quoted_mortgage",
        "frequency": "monthly",
        "description": "Bank/building society standard variable mortgage rate.",
        "unit": "percent per annum",
    },
    "IUMZID4": {
        "name": "Household Deposit Rate – Instant Access ISA",
        "category": "interest_rates",
        "subcategory": "quoted_deposit",
        "frequency": "monthly",
        "description": "Average rate on household instant-access ISA deposits.",
        "unit": "percent per annum",
    },
    "IUMZID2": {
        "name": "Household Deposit Rate – Time Deposits",
        "category": "interest_rates",
        "subcategory": "quoted_deposit",
        "frequency": "monthly",
        "description": "Average rate on household time/fixed-term deposits.",
        "unit": "percent per annum",
    },
    "IUMHPTL": {
        "name": "Personal Loan Rate",
        "category": "interest_rates",
        "subcategory": "quoted_consumer_credit",
        "frequency": "monthly",
        "description": "Quoted personal loan interest rate.",
        "unit": "percent per annum",
    },
    "IUMCCTL": {
        "name": "Credit Card Rate",
        "category": "interest_rates",
        "subcategory": "quoted_consumer_credit",
        "frequency": "monthly",
        "description": "Quoted credit card interest rate.",
        "unit": "percent per annum",
    },
    "IUMODTL": {
        "name": "Overdraft Rate",
        "category": "interest_rates",
        "subcategory": "quoted_consumer_credit",
        "frequency": "monthly",
        "description": "Quoted authorised overdraft interest rate.",
        "unit": "percent per annum",
    },

    # --- Effective Interest Rates (CFM / CFQ prefix) --------------------------
    "CFMZ6KO": {
        "name": "Effective Rate – Mortgage Stock",
        "category": "interest_rates",
        "subcategory": "effective",
        "frequency": "monthly",
        "description": "Effective interest rate on the outstanding stock of household mortgages.",
        "unit": "percent per annum",
    },
    "CFMHSCP": {
        "name": "Effective Rate – Household Deposits (Outstanding)",
        "category": "interest_rates",
        "subcategory": "effective",
        "frequency": "monthly",
        "description": "Effective rate on outstanding household deposits.",
        "unit": "percent per annum",
    },
    "CFMHSCQ": {
        "name": "Effective Rate – Household Deposits (New Business)",
        "category": "interest_rates",
        "subcategory": "effective",
        "frequency": "monthly",
        "description": "Effective rate on new household deposit business.",
        "unit": "percent per annum",
    },

    # --- Monetary Aggregates (LPM prefix) -------------------------------------
    "LPMAUYN": {
        "name": "M4 Liabilities – Total (Seasonally Adjusted, £mn)",
        "category": "monetary_aggregates",
        "subcategory": "m4",
        "frequency": "monthly",
        "description": "Monthly M4 liabilities to private sector – sterling millions, seasonally adjusted.",
        "unit": "GBP millions",
    },
    "LPMAVAA": {
        "name": "Notes & Coin in Circulation – Total (NSA, £mn)",
        "category": "monetary_aggregates",
        "subcategory": "notes_coin",
        "frequency": "monthly",
        "description": "Monthly average total sterling notes and coin in circulation (not seasonally adjusted).",
        "unit": "GBP millions",
    },
    "LPMAVAB": {
        "name": "Notes & Coin in Circulation – Total (SA, £mn)",
        "category": "monetary_aggregates",
        "subcategory": "notes_coin",
        "frequency": "monthly",
        "description": "Notes and coin in circulation excluding Scotland/Northern Ireland backing – seasonally adjusted.",
        "unit": "GBP millions",
    },
    "LPMAVAF": {
        "name": "Notes & Coin – Monthly Change (NSA, £mn)",
        "category": "monetary_aggregates",
        "subcategory": "notes_coin",
        "frequency": "monthly",
        "description": "Monthly changes in total sterling notes and coin in circulation outside the Bank of England (NSA).",
        "unit": "GBP millions",
    },
    "LPMAVAG": {
        "name": "Notes & Coin – Monthly Change (SA, £mn)",
        "category": "monetary_aggregates",
        "subcategory": "notes_coin",
        "frequency": "monthly",
        "description": "Monthly changes in notes and coin outside the Bank of England (seasonally adjusted).",
        "unit": "GBP millions",
    },
    "LPMBL22": {
        "name": "Bank of England Banking Dept Sterling Reserves",
        "category": "monetary_aggregates",
        "subcategory": "boe_balance_sheet",
        "frequency": "monthly",
        "description": "Monthly average amounts outstanding of BoE Banking Department sterling reserves balance liabilities.",
        "unit": "GBP millions",
    },
    "LPMBI2O": {
        "name": "Consumer Credit – Other Lending (Outstanding, £mn)",
        "category": "monetary_aggregates",
        "subcategory": "consumer_credit",
        "frequency": "monthly",
        "description": "Amounts outstanding of consumer credit other than credit cards (includes overdrafts and loans).",
        "unit": "GBP millions",
    },
    "LPMB6VG": {
        "name": "Consumer Credit – Total (Outstanding, £mn)",
        "category": "monetary_aggregates",
        "subcategory": "consumer_credit",
        "frequency": "monthly",
        "description": "Total amounts outstanding of consumer credit.",
        "unit": "GBP millions",
    },

    # --- Quarterly BoE Balance Sheet / CFM prefix (effective rate sub-series) -
    "CFMBI22": {
        "name": "Effective Rate Component – CFM BI22",
        "category": "interest_rates",
        "subcategory": "effective",
        "frequency": "monthly",
        "description": "BoE effective interest rate sub-series (outstanding deposits/loans).",
        "unit": "percent per annum",
    },
    "CFMBI23": {
        "name": "Effective Rate Component – CFM BI23",
        "category": "interest_rates",
        "subcategory": "effective",
        "frequency": "monthly",
        "description": "BoE effective interest rate sub-series (outstanding deposits/loans).",
        "unit": "percent per annum",
    },
    "CFMBJ59": {
        "name": "Effective Rate Component – CFM BJ59",
        "category": "interest_rates",
        "subcategory": "effective",
        "frequency": "monthly",
        "description": "BoE effective interest rate sub-series (new business).",
        "unit": "percent per annum",
    },
    "CFMBJ62": {
        "name": "Effective Rate Component – CFM BJ62",
        "category": "interest_rates",
        "subcategory": "effective",
        "frequency": "monthly",
        "description": "BoE effective interest rate sub-series (new business).",
        "unit": "percent per annum",
    },

    # --- VPQB – national accounts-compatible series ---------------------------
    "VPQB432SI": {
        "name": "Quarterly GDP-Related BoE Series",
        "category": "national_accounts",
        "subcategory": "gdp",
        "frequency": "quarterly",
        "description": "National accounts-compatible quarterly series published in the IADB.",
        "unit": "index / GBP millions (series-dependent)",
    },
}

# Convenience groupings for bulk queries
SERIES_GROUPS: Dict[str, List[str]] = {
    "monetary_policy": ["IUDBEDR", "IUDSOIA", "IUASOIA"],
    "gilt_yields": ["IUDSNPY", "IUDMNPY", "IUDLNPY", "IUDSIZC", "IUDMIZC"],
    "ois_yields": ["IUDZOS2", "IUDZLT2", "IUDZLS6", "IUDZLS7"],
    "gbp_spot_rates": [
        "XUDLUSS", "XUDLERS", "XUDLJYS", "XUDLCDS", "XUDLSFS",
        "XUDLADS", "XUDLNKS", "XUDLSKS", "XUDLDKS", "XUDLSGS",
        "XUDLHDS", "XUDLTWS", "XUDLBK83", "XUDLBK85", "XUDLBK87",
    ],
    "exchange_rate_index": ["XUMAERS", "XUMABRD"],
    "forward_rates": ["XUDLDF1", "XUDLDF3", "XUDLDF6", "XUDLDFY"],
    "quoted_mortgage_rates": [
        "IUMZICQ", "IUMBV34", "IUMZICR", "IUMB482", "IUM2WTL",
        "IUMBV37", "IUMBV42", "IUM5WTL", "IUMBV45", "IUMB479", "IUMBV24",
    ],
    "quoted_deposit_rates": ["IUMZID4", "IUMZID2"],
    "quoted_consumer_credit_rates": ["IUMHPTL", "IUMCCTL", "IUMODTL"],
    "effective_rates": ["CFMZ6KO", "CFMHSCP", "CFMHSCQ"],
    "monetary_aggregates": [
        "LPMAUYN", "LPMAVAA", "LPMAVAB", "LPMAVAF", "LPMAVAG",
        "LPMBL22", "LPMBI2O", "LPMB6VG",
    ],
}


# ---------------------------------------------------------------------------
# Helper utilities
# ---------------------------------------------------------------------------

def _today_str() -> str:
    """Return today's date in BoE format DD/Mon/YYYY."""
    return datetime.now().strftime("%d/%b/%Y")


def _parse_value(raw: str) -> Optional[float]:
    """Convert raw cell string to float; return None for missing ('..' or empty)."""
    if raw is None:
        return None
    stripped = raw.strip()
    if stripped in ("", "..", "n/a", "N/A", "N.A.", "N/A."):
        return None
    try:
        return float(stripped)
    except (ValueError, TypeError):
        return None


def _fetch_csv(
    series_codes: List[str],
    date_from: str,
    date_to: str,
    csvf: str = "TT",
    vpd: str = "Y",
    vfd: str = "N",
) -> Dict[str, Any]:
    """
    Fetch CSV data from the BoE IADB for the given series codes and date range.

    Parameters
    ----------
    series_codes : list of str
        IADB series code identifiers (max 300 per request).
    date_from : str
        Start date in DD/Mon/YYYY format (e.g. '01/Jan/2000').
    date_to : str
        End date in DD/Mon/YYYY format, or the literal string 'now'.
    csvf : str
        CSV format: 'TT' tabular+titles, 'TN' tabular no titles,
        'CT' columnar+titles, 'CN' columnar no titles.
    vpd : str
        'Y' to include provisional data.
    vfd : str
        'Y' to include observation footnotes.

    Returns
    -------
    dict with keys:
        success (bool), data (list of dicts), series_codes (list),
        date_from, date_to, rows_returned (int), source (str)
    """
    if len(series_codes) > 300:
        return {
            "success": False,
            "error": "Maximum 300 series codes per request (use 250 or fewer for safety).",
        }

    params = {
        "csv.x": "yes",
        "Datefrom": date_from,
        "Dateto": date_to,
        "SeriesCodes": ",".join(series_codes),
        "CSVF": csvf,
        "UsingCodes": "Y",
        "VPD": vpd,
        "VFD": vfd,
    }

    try:
        resp = _session.get(BOE_BASE_URL, params=params, timeout=30)
        resp.raise_for_status()

        raw_bytes = resp.content
        # Try UTF-8 first, fall back to latin-1
        try:
            text = raw_bytes.decode("utf-8")
        except UnicodeDecodeError:
            text = raw_bytes.decode("latin-1")

        return _parse_csv_response(text, series_codes, date_from, date_to, csvf)

    except requests.exceptions.Timeout:
        return {"success": False, "error": "Request timed out (30s). Try a shorter date range."}
    except requests.exceptions.HTTPError as exc:
        return {"success": False, "error": f"HTTP {exc.response.status_code}: {str(exc)}"}
    except requests.exceptions.RequestException as exc:
        return {"success": False, "error": f"Network error: {str(exc)}"}


def _parse_csv_response(
    text: str,
    series_codes: List[str],
    date_from: str,
    date_to: str,
    csvf: str,
) -> Dict[str, Any]:
    """
    Parse raw CSV text returned by the BoE IADB into a structured dict.

    The BoE TT (tabular with titles) format has a preamble block before the
    actual data table:

        SERIES,DESCRIPTION           <- literal header of the preamble
        IUDBEDR,Official Bank Rate   <- one row per requested series
        IUDSOIA,Daily SONIA rate
                                     <- blank separator line
        DATE,IUDBEDR,IUDSOIA         <- actual data header
        02 Jan 2025,4.75,4.70        <- data rows ...

    TN (tabular no titles) skips the preamble entirely and starts with the
    DATE header directly.  CT/CN are columnar (code | date | value per row).
    """
    reader = csv.reader(io.StringIO(text))
    rows = list(reader)

    if not rows:
        return {
            "success": False,
            "error": "Empty response from BoE IADB.",
        }

    is_tabular = csvf.upper().startswith("T")
    has_titles = csvf.upper().endswith("T")

    data_records: List[Dict[str, Any]] = []
    series_descriptions: Dict[str, str] = {}

    if is_tabular:
        # Locate the data header row – it begins with "DATE" (case-insensitive).
        data_header_idx: Optional[int] = None
        col_codes: List[str] = []

        for idx, row in enumerate(rows):
            if row and row[0].strip().upper() == "DATE":
                data_header_idx = idx
                col_codes = [c.strip() for c in row[1:] if c.strip()]
                break

        if data_header_idx is None:
            # Fallback: assume first row is header, no preamble
            if rows:
                col_codes = [c.strip() for c in rows[0][1:] if c.strip()] or series_codes[:]
                data_header_idx = 0
            else:
                return {"success": False, "error": "Could not locate data header row."}

        # If has_titles, the rows before data_header_idx are the SERIES/DESCRIPTION preamble
        if has_titles:
            in_preamble = False
            for row in rows[:data_header_idx]:
                if not row or not row[0].strip():
                    continue
                if row[0].strip().upper() == "SERIES":
                    in_preamble = True
                    continue
                if in_preamble and len(row) >= 2:
                    series_descriptions[row[0].strip()] = row[1].strip()

        # If col_codes is empty (e.g. one-series TT), fall back to supplied codes
        if not col_codes:
            col_codes = series_codes[:]

        for row in rows[data_header_idx + 1:]:
            if not row or not row[0].strip():
                continue
            date_val = row[0].strip()
            # Skip any residual header/separator rows
            if date_val.upper() == "DATE":
                continue
            record: Dict[str, Any] = {"date": date_val}
            for i, code in enumerate(col_codes):
                cell = row[i + 1] if (i + 1) < len(row) else ""
                record[code] = _parse_value(cell)
            data_records.append(record)

    else:
        # Columnar (CT / CN): code | date | value
        header_offset = 1 if has_titles else 0
        for row in rows[header_offset:]:
            if len(row) < 3 or not row[0].strip():
                continue
            code = row[0].strip()
            date_val = row[1].strip()
            val = _parse_value(row[2])
            data_records.append({"series_code": code, "date": date_val, "value": val})

    result: Dict[str, Any] = {
        "success": True,
        "data": data_records,
        "series_codes": series_codes,
        "date_from": date_from,
        "date_to": date_to,
        "rows_returned": len(data_records),
        "source": "Bank of England IADB",
        "url": "https://www.bankofengland.co.uk/boeapps/database/",
        "notes": (
            "Null/None values represent missing observations (originally '..' in BoE CSV). "
            "Data represent indicative mid-market rates unless stated otherwise. "
            "Exchange rates are as observed by the Bank's FX Desk at approx. 4pm London time."
        ),
    }
    if series_descriptions:
        result["series_descriptions"] = series_descriptions
    return result


# ---------------------------------------------------------------------------
# High-level API methods
# ---------------------------------------------------------------------------

class BankOfEnglandWrapper:
    """
    Full-featured Python wrapper for the Bank of England Statistical Interactive
    Database (IADB).

    Authentication: None required. The IADB is a free, public data service.
    Rate limits   : Not officially published. Avoid batching more than 300 codes
                    per request and keep concurrent requests modest.
    """

    def __init__(
        self,
        default_start: str = "01/Jan/1970",
        default_end: str = "now",
    ) -> None:
        self.default_start = default_start
        self.default_end = default_end

    # ------------------------------------------------------------------
    # Core fetch methods
    # ------------------------------------------------------------------

    def get_series(
        self,
        series_codes: Union[str, List[str]],
        start: Optional[str] = None,
        end: Optional[str] = None,
        csvf: str = "TT",
    ) -> Dict[str, Any]:
        """
        Fetch one or more IADB series by code.

        Parameters
        ----------
        series_codes : str or list of str
            One or more 7-character IADB codes (comma-separated string or list).
        start : str, optional
            Start date DD/Mon/YYYY. Defaults to instance default_start.
        end : str, optional
            End date DD/Mon/YYYY or 'now'. Defaults to instance default_end.
        csvf : str
            Output layout: 'TT' (tabular+headers, default), 'TN', 'CT', 'CN'.

        Returns
        -------
        dict
            success, data (list of observation dicts), series_codes, metadata.
        """
        if isinstance(series_codes, str):
            codes = [c.strip() for c in series_codes.split(",") if c.strip()]
        else:
            codes = list(series_codes)

        date_from = start or self.default_start
        date_to = end or self.default_end

        result = _fetch_csv(codes, date_from, date_to, csvf=csvf)

        # Enrich with catalogue metadata
        if result.get("success"):
            result["series_metadata"] = {
                code: SERIES_CATALOGUE.get(code, {"name": code, "description": "Unknown series"})
                for code in codes
            }

        return result

    def get_series_group(
        self,
        group_name: str,
        start: Optional[str] = None,
        end: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        Fetch a pre-defined group of related series.

        Available groups: monetary_policy, gilt_yields, ois_yields,
        gbp_spot_rates, exchange_rate_index, forward_rates,
        quoted_mortgage_rates, quoted_deposit_rates,
        quoted_consumer_credit_rates, effective_rates, monetary_aggregates.
        """
        if group_name not in SERIES_GROUPS:
            return {
                "success": False,
                "error": f"Unknown group '{group_name}'. Available: {list(SERIES_GROUPS.keys())}",
            }
        codes = SERIES_GROUPS[group_name]
        result = self.get_series(codes, start=start, end=end)
        result["group"] = group_name
        return result

    # ------------------------------------------------------------------
    # Convenience methods (by topic)
    # ------------------------------------------------------------------

    def get_bank_rate(
        self,
        start: Optional[str] = None,
        end: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Fetch the Official Bank Rate (IUDBEDR) history."""
        result = self.get_series(["IUDBEDR"], start=start, end=end)
        result["description"] = "Bank of England Official Bank Rate set by the MPC"
        return result

    def get_sonia(
        self,
        start: Optional[str] = None,
        end: Optional[str] = None,
        include_compounded_index: bool = False,
    ) -> Dict[str, Any]:
        """
        Fetch SONIA (Sterling Overnight Index Average) daily rate.

        Parameters
        ----------
        include_compounded_index : bool
            If True, also fetches the SONIA Compounded Index (IUASOIA).
        """
        codes = ["IUDSOIA"]
        if include_compounded_index:
            codes.append("IUASOIA")
        result = self.get_series(codes, start=start, end=end)
        result["description"] = "Sterling Overnight Index Average (SONIA)"
        return result

    def get_exchange_rates(
        self,
        currencies: Optional[List[str]] = None,
        start: Optional[str] = None,
        end: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        Fetch GBP spot exchange rates against major currencies.

        Parameters
        ----------
        currencies : list of str, optional
            Subset of currency codes e.g. ['USD', 'EUR', 'JPY'].
            If None, fetches all available GBP spot series.
        """
        currency_to_code: Dict[str, str] = {
            "USD": "XUDLUSS",
            "EUR": "XUDLERS",
            "JPY": "XUDLJYS",
            "CAD": "XUDLCDS",
            "CHF": "XUDLSFS",
            "AUD": "XUDLADS",
            "NOK": "XUDLNKS",
            "SEK": "XUDLSKS",
            "DKK": "XUDLDKS",
            "SGD": "XUDLSGS",
            "HKD": "XUDLHDS",
            "TWD": "XUDLTWS",
            "MYR": "XUDLBK25",
            "INR": "XUDLBK83",
            "KRW": "XUDLBK85",
            "ZAR": "XUDLBK87",
        }

        if currencies:
            unknown = [c for c in currencies if c.upper() not in currency_to_code]
            if unknown:
                return {
                    "success": False,
                    "error": f"Unknown currency codes: {unknown}. "
                             f"Available: {list(currency_to_code.keys())}",
                }
            codes = [currency_to_code[c.upper()] for c in currencies]
        else:
            codes = SERIES_GROUPS["gbp_spot_rates"]

        result = self.get_series(codes, start=start, end=end)
        result["description"] = (
            "GBP spot exchange rates – indicative mid-market rates observed by the "
            "Bank of England FX Desk at approximately 4pm London time. "
            "Not official rates; published for reference."
        )
        result["base_currency"] = "GBP"
        return result

    def get_exchange_rate_index(
        self,
        start: Optional[str] = None,
        end: Optional[str] = None,
        broad: bool = False,
    ) -> Dict[str, Any]:
        """
        Fetch the Sterling Exchange Rate Index (ERI).

        Parameters
        ----------
        broad : bool
            If True, also fetches the broad ERI (XUMABRD) in addition to narrow (XUMAERS).
        """
        codes = ["XUMAERS"]
        if broad:
            codes.append("XUMABRD")
        result = self.get_series(codes, start=start, end=end)
        result["description"] = (
            "Sterling trade-weighted Exchange Rate Index (ERI). "
            "Jan 2005 = 100. Annually re-weighted with world trade data."
        )
        return result

    def get_quoted_rates(
        self,
        include_mortgages: bool = True,
        include_deposits: bool = True,
        include_consumer_credit: bool = True,
        start: Optional[str] = None,
        end: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        Fetch quoted household interest rates (mortgages, deposits, consumer credit).
        Coverage begins from approximately January 2000 (mortgage data) or January 2010
        (combined bank + building society data).
        """
        codes: List[str] = []
        if include_mortgages:
            codes.extend(SERIES_GROUPS["quoted_mortgage_rates"])
        if include_deposits:
            codes.extend(SERIES_GROUPS["quoted_deposit_rates"])
        if include_consumer_credit:
            codes.extend(SERIES_GROUPS["quoted_consumer_credit_rates"])

        if not codes:
            return {"success": False, "error": "No rate categories selected."}

        result = self.get_series(codes, start=start, end=end)
        result["description"] = (
            "Quoted household interest rates published monthly by the Bank of England. "
            "From January 2010, rates cover combined bank and building society data."
        )
        return result

    def get_effective_rates(
        self,
        start: Optional[str] = None,
        end: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        Fetch effective interest rates (weighted average rates on outstanding balances
        and new business for deposits and loans).
        """
        result = self.get_series_group("effective_rates", start=start, end=end)
        result["description"] = (
            "Effective interest rates: average rates actually paid or received by "
            "households and companies on outstanding stock and new business."
        )
        return result

    def get_monetary_aggregates(
        self,
        start: Optional[str] = None,
        end: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        Fetch UK monetary aggregate data including M4, notes & coin in circulation,
        consumer credit, and BoE reserve balance liabilities.
        M4 data available monthly from June 1982.
        """
        result = self.get_series_group("monetary_aggregates", start=start, end=end)
        result["description"] = (
            "UK monetary aggregates: M4 liabilities, notes and coin in circulation, "
            "consumer credit outstanding, and BoE Banking Department sterling reserves."
        )
        return result

    def get_yield_curves(
        self,
        curve_type: str = "gilt_nominal",
        start: Optional[str] = None,
        end: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        Fetch yield curve data from the IADB.

        Parameters
        ----------
        curve_type : str
            One of: 'gilt_nominal', 'gilt_inflation', 'ois'.
            Note: The full term-structure yield curve data (per-maturity) is published
            as ZIP downloads on the statistics/yield-curves page, not via IADB series
            codes. The IADB series returned here are key-maturity spot/par yields.
        """
        group_map = {
            "gilt_nominal": "gilt_yields",
            "gilt_inflation": "gilt_yields",  # IUDSIZC, IUDMIZC
            "ois": "ois_yields",
        }
        if curve_type not in group_map:
            return {
                "success": False,
                "error": f"Unknown curve_type '{curve_type}'. "
                         f"Choose from: {list(group_map.keys())}",
            }

        if curve_type == "gilt_inflation":
            codes = ["IUDSIZC", "IUDMIZC"]
            result = self.get_series(codes, start=start, end=end)
        else:
            result = self.get_series_group(group_map[curve_type], start=start, end=end)

        result["description"] = (
            f"UK yield curve data – {curve_type}. "
            "Continuously compounded, quoted on annual basis. "
            "Full term-structure ZIP downloads available at "
            "https://www.bankofengland.co.uk/statistics/yield-curves"
        )
        result["yield_curve_zip_downloads"] = {
            "nominal_government": "https://www.bankofengland.co.uk/statistics/yield-curves",
            "real_government": "https://www.bankofengland.co.uk/statistics/yield-curves",
            "inflation_government": "https://www.bankofengland.co.uk/statistics/yield-curves",
            "ois": "https://www.bankofengland.co.uk/statistics/yield-curves",
        }
        return result

    def get_overview(self) -> Dict[str, Any]:
        """
        Fetch a concise snapshot of the most important BoE series for a
        quick dashboard view (last 3 months of data).
        """
        # Use a short recent window to keep the response fast
        end = "now"
        start = "01/Jan/2024"

        snapshot_codes = [
            "IUDBEDR",   # Bank Rate
            "IUDSOIA",   # SONIA
            "XUDLUSS",   # GBP/USD
            "XUDLERS",   # GBP/EUR
            "XUDLJYS",   # GBP/JPY
            "XUMAERS",   # Sterling ERI
            "IUDMNPY",   # 10Y Gilt yield
            "IUDZLS6",   # OIS 10Y
        ]

        result = self.get_series(snapshot_codes, start=start, end=end)
        result["description"] = (
            "Bank of England IADB overview snapshot – key rates and exchange rates."
        )
        result["available_groups"] = list(SERIES_GROUPS.keys())
        result["total_documented_series"] = len(SERIES_CATALOGUE)
        return result

    # ------------------------------------------------------------------
    # Discovery helpers
    # ------------------------------------------------------------------

    def get_available_categories(self) -> Dict[str, Any]:
        """Return the full series catalogue and group listings."""
        categories: Dict[str, List[Dict]] = {}
        for code, meta in SERIES_CATALOGUE.items():
            cat = meta.get("category", "other")
            if cat not in categories:
                categories[cat] = []
            categories[cat].append({"code": code, **meta})

        return {
            "success": True,
            "data": {
                "categories": categories,
                "series_groups": {
                    name: {
                        "codes": codes,
                        "count": len(codes),
                    }
                    for name, codes in SERIES_GROUPS.items()
                },
                "total_series": len(SERIES_CATALOGUE),
            },
            "api_details": {
                "base_url": BOE_BASE_URL,
                "authentication": "None required",
                "response_formats": ["CSV", "XML", "HTML", "Excel"],
                "primary_format": "CSV",
                "date_format": "DD/Mon/YYYY (e.g. 01/Jan/2000)",
                "special_date_values": {"end_date": "Use 'now' for latest available data"},
                "max_codes_per_request": {
                    "CSV": 300,
                    "HTML": 300,
                    "XML": 300,
                    "Excel": 250,
                },
                "csvf_options": {
                    "TT": "Tabular layout with column header titles",
                    "TN": "Tabular layout without titles",
                    "CT": "Columnar layout with titles",
                    "CN": "Columnar layout without titles",
                },
                "parameters": {
                    "csv.x": "yes (activates CSV mode)",
                    "Datefrom": "Start date DD/Mon/YYYY",
                    "Dateto": "End date DD/Mon/YYYY or 'now'",
                    "SeriesCodes": "Comma-separated IADB series codes",
                    "CSVF": "CSV format (TT/TN/CT/CN)",
                    "UsingCodes": "Y (mandatory when using SeriesCodes)",
                    "VPD": "Y/N – include provisional data",
                    "VFD": "Y/N – include observation footnotes",
                },
                "xml_only_parameters": {
                    "CodeVer": "new (required for XML mode)",
                    "xml.x": "yes",
                    "VUD": "A=all observations, B=updated only",
                    "VUDdate": "DD/Mon/YYYY or 'latest' – filter updates since date",
                },
                "missing_value_codes": {
                    "..": "Data not available for this period",
                    "N/a": "Not applicable",
                },
                "data_update_schedule": "Aim: by 10am on publication day; daily rates 1-2 working days after reporting date",
                "historical_depth": "Some daily series back to 1975; M4 monthly from June 1982",
                "contact": "BEEDSQueries@bankofengland.co.uk",
                "database_home": "https://www.bankofengland.co.uk/boeapps/database/",
                "help_page": "https://www.bankofengland.co.uk/boeapps/database/help.asp",
                "statistics_home": "https://www.bankofengland.co.uk/statistics",
            },
            "source": "Bank of England IADB",
        }


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

def _parse_cli_args(argv: List[str]) -> Dict[str, Any]:
    """Very simple key=value / --key value argument parser."""
    parsed: Dict[str, Any] = {}
    i = 0
    while i < len(argv):
        tok = argv[i]
        if tok.startswith("--"):
            key = tok[2:]
            if "=" in key:
                k, v = key.split("=", 1)
                parsed[k] = v
            elif i + 1 < len(argv) and not argv[i + 1].startswith("--"):
                parsed[key] = argv[i + 1]
                i += 1
            else:
                parsed[key] = True
        i += 1
    return parsed


def main(args: Optional[List[str]] = None) -> None:
    if args is None:
        args = sys.argv[1:]

    if not args:
        print(json.dumps({
            "success": False,
            "error": "No command supplied.",
            "usage": "python boe_data.py <command> [options]",
            "commands": [
                "available_categories",
                "overview",
                "bank_rate          [--start DD/Mon/YYYY] [--end DD/Mon/YYYY|now]",
                "sonia              [--start ...] [--end ...] [--compounded Y]",
                "exchange_rates     [--currencies USD,EUR,JPY,...] [--start ...] [--end ...]",
                "exchange_rate_index [--broad Y] [--start ...] [--end ...]",
                "quoted_rates       [--start ...] [--end ...]",
                "effective_rates    [--start ...] [--end ...]",
                "monetary_aggregates [--start ...] [--end ...]",
                "yield_curves       [--type gilt_nominal|gilt_inflation|ois] [--start ...] [--end ...]",
                "group              --name <group_name> [--start ...] [--end ...]",
                "series             --codes CODE1,CODE2,... [--start ...] [--end ...]",
            ],
        }, indent=2))
        sys.exit(1)

    command = args[0]
    kwargs = _parse_cli_args(args[1:])
    wrapper = BankOfEnglandWrapper()

    try:
        start = kwargs.get("start")
        end = kwargs.get("end")

        if command == "available_categories":
            result = wrapper.get_available_categories()

        elif command == "overview":
            result = wrapper.get_overview()

        elif command == "bank_rate":
            result = wrapper.get_bank_rate(start=start, end=end)

        elif command == "sonia":
            compounded = str(kwargs.get("compounded", "N")).upper() == "Y"
            result = wrapper.get_sonia(
                start=start, end=end, include_compounded_index=compounded
            )

        elif command == "exchange_rates":
            raw_currencies = kwargs.get("currencies")
            currencies = (
                [c.strip().upper() for c in raw_currencies.split(",")]
                if raw_currencies else None
            )
            result = wrapper.get_exchange_rates(
                currencies=currencies, start=start, end=end
            )

        elif command == "exchange_rate_index":
            broad = str(kwargs.get("broad", "N")).upper() == "Y"
            result = wrapper.get_exchange_rate_index(start=start, end=end, broad=broad)

        elif command == "quoted_rates":
            result = wrapper.get_quoted_rates(start=start, end=end)

        elif command == "effective_rates":
            result = wrapper.get_effective_rates(start=start, end=end)

        elif command == "monetary_aggregates":
            result = wrapper.get_monetary_aggregates(start=start, end=end)

        elif command == "yield_curves":
            curve_type = kwargs.get("type", "gilt_nominal")
            result = wrapper.get_yield_curves(
                curve_type=curve_type, start=start, end=end
            )

        elif command == "group":
            name = kwargs.get("name")
            if not name:
                result = {
                    "success": False,
                    "error": "Provide --name <group_name>. "
                             f"Available: {list(SERIES_GROUPS.keys())}",
                }
            else:
                result = wrapper.get_series_group(name, start=start, end=end)

        elif command == "series":
            raw_codes = kwargs.get("codes", "")
            if not raw_codes:
                result = {
                    "success": False,
                    "error": "Provide --codes CODE1,CODE2,... (e.g. --codes IUDBEDR,IUDSOIA)",
                }
            else:
                codes = [c.strip() for c in raw_codes.split(",") if c.strip()]
                result = wrapper.get_series(codes, start=start, end=end)

        else:
            result = {
                "success": False,
                "error": f"Unknown command: '{command}'. Run without arguments for usage.",
            }

        print(json.dumps(result, indent=2, default=str))

    except KeyboardInterrupt:
        sys.exit(0)
    except Exception as exc:
        print(json.dumps({"success": False, "error": str(exc)}, indent=2))
        sys.exit(1)


if __name__ == "__main__":
    main()
