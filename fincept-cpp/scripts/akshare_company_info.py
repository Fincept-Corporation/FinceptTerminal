"""
AKShare Company Information API
Fetches detailed company information for stocks
"""

import sys
import json
import akshare as ak
import pandas as pd
from datetime import datetime

def get_cn_basic_info(symbol):
    """Get basic info for Chinese A-share stock"""
    try:
        df = ak.stock_individual_info_em(symbol=symbol)
        # Convert to dict with proper key-value pairs
        result = {}
        for _, row in df.iterrows():
            key = str(row['item'])
            value = row['value']
            result[key] = value

        return {
            "success": True,
            "data": result,
            "timestamp": int(datetime.now().timestamp())
        }
    except Exception as e:
        return {
            "success": False,
            "error": str(e),
            "timestamp": int(datetime.now().timestamp())
        }

def get_cn_profile(symbol):
    """Get detailed profile for Chinese A-share stock"""
    try:
        df = ak.stock_profile_cninfo(symbol=symbol)
        if df.empty:
            return {
                "success": False,
                "error": "No data found",
                "timestamp": int(datetime.now().timestamp())
            }

        # Convert first row to dict
        result = df.iloc[0].to_dict()

        return {
            "success": True,
            "data": result,
            "timestamp": int(datetime.now().timestamp())
        }
    except Exception as e:
        return {
            "success": False,
            "error": str(e),
            "timestamp": int(datetime.now().timestamp())
        }

def get_hk_profile(symbol):
    """Get profile for Hong Kong stock"""
    try:
        df = ak.stock_hk_company_profile_em(symbol=symbol)
        if df.empty:
            return {
                "success": False,
                "error": "No data found",
                "timestamp": int(datetime.now().timestamp())
            }

        # Convert first row to dict
        result = df.iloc[0].to_dict()

        return {
            "success": True,
            "data": result,
            "timestamp": int(datetime.now().timestamp())
        }
    except Exception as e:
        return {
            "success": False,
            "error": str(e),
            "timestamp": int(datetime.now().timestamp())
        }

def get_us_info(symbol):
    """Get info for US stock (from Xueqiu - may require token)"""
    try:
        df = ak.stock_individual_basic_info_us_xq(symbol=symbol)
        if df.empty:
            return {
                "success": False,
                "error": "No data found",
                "timestamp": int(datetime.now().timestamp())
            }

        # Convert first row to dict
        result = df.iloc[0].to_dict()

        return {
            "success": True,
            "data": result,
            "timestamp": int(datetime.now().timestamp())
        }
    except Exception as e:
        return {
            "success": False,
            "error": str(e),
            "timestamp": int(datetime.now().timestamp())
        }

def get_all_endpoints():
    """Get list of all available endpoints"""
    endpoints = ["cn_basic", "cn_profile", "hk_profile", "us_info"]
    return {
        "success": True,
        "data": {
            "available_endpoints": endpoints,
            "total_count": len(endpoints),
            "categories": {
                "CN Stocks": ["cn_basic", "cn_profile"],
                "HK Stocks": ["hk_profile"],
                "US Stocks": ["us_info"]
            },
            "timestamp": int(datetime.now().timestamp())
        },
        "count": len(endpoints),
        "timestamp": int(datetime.now().timestamp())
    }

def main():
    # Reconfigure stdout to UTF-8
    if sys.platform == 'win32':
        import io
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

    if len(sys.argv) < 2:
        print(json.dumps({
            "success": False,
            "error": "Missing endpoint parameter"
        }))
        sys.exit(1)

    endpoint = sys.argv[1]

    # Handle get_all_endpoints without symbol
    if endpoint == "get_all_endpoints":
        result = get_all_endpoints()
    elif len(sys.argv) < 3:
        print(json.dumps({
            "success": False,
            "error": "Missing symbol parameter"
        }))
        sys.exit(1)
    else:
        symbol = sys.argv[2]
        if endpoint == "cn_basic":
            result = get_cn_basic_info(symbol)
        elif endpoint == "cn_profile":
            result = get_cn_profile(symbol)
        elif endpoint == "hk_profile":
            result = get_hk_profile(symbol)
        elif endpoint == "us_info":
            result = get_us_info(symbol)
        else:
            result = {
                "success": False,
                "error": f"Unknown endpoint: {endpoint}"
            }

    print(json.dumps(result, ensure_ascii=True, default=str))

if __name__ == "__main__":
    main()
