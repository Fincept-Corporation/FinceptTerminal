#!/usr/bin/env python3
"""
AKShare Stocks Hot & Special Data Wrapper (Batch 8)
Provides access to hot stocks, news, comments, ESG, notices, and miscellaneous data
~50 endpoints
"""

import sys
import json
import time
from datetime import datetime

try:
    import akshare as ak
    import pandas as pd
except ImportError as e:
    print(json.dumps({
        "success": False,
        "error": f"Missing dependency: {e}",
        "data": []
    }))
    sys.exit(1)


def safe_call(func, *args, **kwargs):
    """Safely call AKShare function with error handling and retries"""
    max_retries = 2
    for attempt in range(max_retries):
        try:
            result = func(*args, **kwargs)
            if isinstance(result, pd.DataFrame):
                if result.empty:
                    return {"success": True, "data": [], "count": 0}
                for col in result.columns:
                    if result[col].dtype == 'datetime64[ns]':
                        result[col] = result[col].astype(str)
                # Replace NaN/Infinity with None for valid JSON
                result = result.replace([float("inf"), float("-inf")], None)
                result = result.where(pd.notna(result), None)
                data = result.to_dict(orient='records')
                return {"success": True, "data": data, "count": len(data)}
            elif isinstance(result, (list, dict)):
                return {"success": True, "data": result, "count": len(result) if isinstance(result, list) else 1}
            else:
                return {"success": True, "data": str(result), "count": 1}
        except Exception as e:
            if attempt < max_retries - 1:
                time.sleep(1)
                continue
            return {"success": False, "error": str(e), "data": []}
    return {"success": False, "error": "Max retries exceeded", "data": []}


# ==================== HOT STOCKS (热门股票) ====================

def get_stock_hot_rank_em():
    """Get hot stock ranking (EastMoney)"""
    return safe_call(ak.stock_hot_rank_em)

def get_stock_hot_rank_detail_em(symbol="000001"):
    """Get hot stock ranking detail (EastMoney)"""
    return safe_call(ak.stock_hot_rank_detail_em, symbol=symbol)

def get_stock_hot_rank_detail_realtime_em(symbol="000001"):
    """Get hot stock ranking realtime detail (EastMoney)"""
    return safe_call(ak.stock_hot_rank_detail_realtime_em, symbol=symbol)

def get_stock_hot_rank_latest_em(symbol="A股"):
    """Get hot stock ranking latest (EastMoney)"""
    return safe_call(ak.stock_hot_rank_latest_em, symbol=symbol)

def get_stock_hot_rank_relate_em(symbol="000001"):
    """Get hot stock related stocks (EastMoney)"""
    return safe_call(ak.stock_hot_rank_relate_em, symbol=symbol)

def get_stock_hot_up_em():
    """Get hot up stocks (EastMoney)"""
    return safe_call(ak.stock_hot_up_em)

def get_stock_hot_keyword_em(symbol="A股"):
    """Get hot keywords (EastMoney)"""
    return safe_call(ak.stock_hot_keyword_em, symbol=symbol)

def get_stock_hot_search_baidu(date="20240101"):
    """Get hot search (Baidu)"""
    return safe_call(ak.stock_hot_search_baidu, date=date)


# ==================== HK HOT STOCKS ====================

def get_stock_hk_hot_rank_em():
    """Get HK hot stock ranking (EastMoney)"""
    return safe_call(ak.stock_hk_hot_rank_em)

def get_stock_hk_hot_rank_detail_em(symbol="00700"):
    """Get HK hot stock detail (EastMoney)"""
    return safe_call(ak.stock_hk_hot_rank_detail_em, symbol=symbol)

def get_stock_hk_hot_rank_detail_realtime_em(symbol="00700"):
    """Get HK hot stock realtime (EastMoney)"""
    return safe_call(ak.stock_hk_hot_rank_detail_realtime_em, symbol=symbol)

def get_stock_hk_hot_rank_latest_em():
    """Get HK hot stock latest (EastMoney)"""
    return safe_call(ak.stock_hk_hot_rank_latest_em)


# ==================== XUEQIU HOT ====================

def get_stock_hot_deal_xq():
    """Get hot deal (XueQiu)"""
    return safe_call(ak.stock_hot_deal_xq)

def get_stock_hot_follow_xq(symbol="全球"):
    """Get hot follow (XueQiu)"""
    return safe_call(ak.stock_hot_follow_xq, symbol=symbol)

def get_stock_hot_tweet_xq(symbol="全球"):
    """Get hot tweet (XueQiu)"""
    return safe_call(ak.stock_hot_tweet_xq, symbol=symbol)

def get_stock_inner_trade_xq(symbol="SZ000001"):
    """Get inner trade (XueQiu)"""
    return safe_call(ak.stock_inner_trade_xq, symbol=symbol)


# ==================== NEWS & ANNOUNCEMENTS ====================

def get_stock_news_em(symbol="000001"):
    """Get stock news (EastMoney)"""
    return safe_call(ak.stock_news_em, symbol=symbol)

def get_stock_news_main_cx():
    """Get main stock news (CaiXin)"""
    return safe_call(ak.stock_news_main_cx)

def get_stock_notice_report(symbol="全部", date=""):
    """Get notice report"""
    return safe_call(ak.stock_notice_report, symbol=symbol, date=date)

def get_stock_report_disclosure():
    """Get report disclosure"""
    return safe_call(ak.stock_report_disclosure)


# ==================== COMMENTS & SENTIMENT ====================

def get_stock_comment_em():
    """Get stock comment (EastMoney)"""
    return safe_call(ak.stock_comment_em)

def get_stock_comment_detail_scrd_desire_em(symbol="000001"):
    """Get stock comment desire detail (EastMoney)"""
    return safe_call(ak.stock_comment_detail_scrd_desire_em, symbol=symbol)

def get_stock_comment_detail_scrd_desire_daily_em(symbol="000001"):
    """Get stock comment desire daily (EastMoney)"""
    return safe_call(ak.stock_comment_detail_scrd_desire_daily_em, symbol=symbol)

def get_stock_comment_detail_scrd_focus_em(symbol="000001"):
    """Get stock comment focus detail (EastMoney)"""
    return safe_call(ak.stock_comment_detail_scrd_focus_em, symbol=symbol)

def get_stock_comment_detail_zhpj_lspf_em(symbol="000001"):
    """Get stock comment history rating (EastMoney)"""
    return safe_call(ak.stock_comment_detail_zhpj_lspf_em, symbol=symbol)

def get_stock_comment_detail_zlkp_jgcyd_em(symbol="000001"):
    """Get stock comment institutional participation (EastMoney)"""
    return safe_call(ak.stock_comment_detail_zlkp_jgcyd_em, symbol=symbol)


# ==================== WEIBO SENTIMENT ====================

def get_stock_js_weibo_report(time_period="CNHOUR12"):
    """Get Weibo stock report
    time_period: CNHOUR2, CNHOUR6, CNHOUR12, CNHOUR24, CNDAY7, CNDAY30
    """
    return safe_call(ak.stock_js_weibo_report, time_period=time_period)

def get_stock_js_weibo_nlp_time():
    """Get Weibo NLP time data (no parameters required)"""
    return safe_call(ak.stock_js_weibo_nlp_time)


# ==================== ESG DATA ====================

def get_stock_esg_rate_sina():
    """Get ESG rating (Sina)"""
    return safe_call(ak.stock_esg_rate_sina)

def get_stock_esg_hz_sina(symbol="000001"):
    """Get ESG HZ data (Sina)"""
    return safe_call(ak.stock_esg_hz_sina, symbol=symbol)

def get_stock_esg_msci_sina():
    """Get MSCI ESG data (Sina)"""
    return safe_call(ak.stock_esg_msci_sina)

def get_stock_esg_rft_sina():
    """Get ESG RFT data (Sina)"""
    return safe_call(ak.stock_esg_rft_sina)

def get_stock_esg_zd_sina():
    """Get ESG ZD data (Sina)"""
    return safe_call(ak.stock_esg_zd_sina)


# ==================== IRM (投资者关系) ====================

def get_stock_irm_cninfo(symbol="000001"):
    """Get investor relations (CNINFO)"""
    return safe_call(ak.stock_irm_cninfo, symbol=symbol)

def get_stock_irm_ans_cninfo(symbol="000001"):
    """Get investor relations answers (CNINFO)"""
    return safe_call(ak.stock_irm_ans_cninfo, symbol=symbol)


# ==================== SNS INFO ====================

def get_stock_sns_sseinfo(symbol="000001"):
    """Get SNS info from SSE"""
    return safe_call(ak.stock_sns_sseinfo, symbol=symbol)


# ==================== DISCLOSURE ====================

def get_stock_zh_a_disclosure_relation_cninfo(symbol="000001"):
    """Get disclosure relation (CNINFO)"""
    return safe_call(ak.stock_zh_a_disclosure_relation_cninfo, symbol=symbol)

def get_stock_zh_a_disclosure_report_cninfo(symbol="000001", start_date="2024-01-01", end_date="2024-12-31"):
    """Get disclosure report (CNINFO)"""
    return safe_call(ak.stock_zh_a_disclosure_report_cninfo, symbol=symbol, start_date=start_date, end_date=end_date)


# ==================== SPECIAL DATA ====================

def get_stock_yysj_em(date="20241231"):
    """Get appointment date (EastMoney)"""
    return safe_call(ak.stock_yysj_em, date=date)

def get_stock_yzxdr_em():
    """Get major event (EastMoney)"""
    return safe_call(ak.stock_yzxdr_em)

def get_stock_zdhtmx_em(symbol="全部"):
    """Get major contract (EastMoney)"""
    return safe_call(ak.stock_zdhtmx_em, symbol=symbol)

def get_stock_zygc_em(symbol="000001"):
    """Get main business composition (EastMoney)"""
    return safe_call(ak.stock_zygc_em, symbol=symbol)

def get_stock_gsrl_gsdt_em(symbol="000001"):
    """Get company schedule (EastMoney)"""
    return safe_call(ak.stock_gsrl_gsdt_em, symbol=symbol)

def get_stock_info_cjzc_em():
    """Get new stock info (EastMoney)"""
    return safe_call(ak.stock_info_cjzc_em)

def get_stock_changes_em(symbol="大笔买入"):
    """Get stock changes (EastMoney)"""
    return safe_call(ak.stock_changes_em, symbol=symbol)

def get_stock_qsjy_em(symbol="000001"):
    """Get warrant data (EastMoney)"""
    return safe_call(ak.stock_qsjy_em, symbol=symbol)

def get_stock_qbzf_em():
    """Get private placement data (EastMoney)"""
    return safe_call(ak.stock_qbzf_em)

def get_stock_tfp_em():
    """Get suspension data (EastMoney)"""
    return safe_call(ak.stock_tfp_em)

def get_stock_sy_em(symbol="上海市"):
    """Get stocks by province (EastMoney)"""
    return safe_call(ak.stock_sy_em, symbol=symbol)

def get_stock_sy_hy_em(symbol="证监会"):
    """Get stocks by industry (EastMoney)"""
    return safe_call(ak.stock_sy_hy_em, symbol=symbol)

def get_stock_sy_jz_em():
    """Get stocks by fund (EastMoney)"""
    return safe_call(ak.stock_sy_jz_em)

def get_stock_sy_profile_em(symbol="000001"):
    """Get stock profile (EastMoney)"""
    return safe_call(ak.stock_sy_profile_em, symbol=symbol)

def get_stock_sy_yq_em(symbol="000001"):
    """Get stock forecast (EastMoney)"""
    return safe_call(ak.stock_sy_yq_em, symbol=symbol)


# ==================== VOTE DATA ====================

def get_stock_zh_vote_baidu(symbol="000001"):
    """Get stock vote (Baidu)"""
    return safe_call(ak.stock_zh_vote_baidu, symbol=symbol)


# ==================== MANAGEMENT CHANGE ====================

def get_stock_management_change_ths(symbol="000001"):
    """Get management change (THS)"""
    return safe_call(ak.stock_management_change_ths, symbol=symbol)


# ==================== ENDPOINT REGISTRY ====================

ENDPOINTS = {
    # Hot Stocks
    "stock_hot_rank_em": {"func": get_stock_hot_rank_em, "desc": "Hot stock ranking", "category": "Hot Stocks"},
    "stock_hot_rank_detail_em": {"func": get_stock_hot_rank_detail_em, "desc": "Hot stock detail", "category": "Hot Stocks"},
    "stock_hot_rank_detail_realtime_em": {"func": get_stock_hot_rank_detail_realtime_em, "desc": "Hot stock realtime", "category": "Hot Stocks"},
    "stock_hot_rank_latest_em": {"func": get_stock_hot_rank_latest_em, "desc": "Hot stock latest", "category": "Hot Stocks"},
    "stock_hot_rank_relate_em": {"func": get_stock_hot_rank_relate_em, "desc": "Hot stock related", "category": "Hot Stocks"},
    "stock_hot_up_em": {"func": get_stock_hot_up_em, "desc": "Hot up stocks", "category": "Hot Stocks"},
    "stock_hot_keyword_em": {"func": get_stock_hot_keyword_em, "desc": "Hot keywords", "category": "Hot Stocks"},
    "stock_hot_search_baidu": {"func": get_stock_hot_search_baidu, "desc": "Hot search (Baidu)", "category": "Hot Stocks"},

    # HK Hot Stocks
    "stock_hk_hot_rank_em": {"func": get_stock_hk_hot_rank_em, "desc": "HK hot ranking", "category": "HK Hot Stocks"},
    "stock_hk_hot_rank_detail_em": {"func": get_stock_hk_hot_rank_detail_em, "desc": "HK hot detail", "category": "HK Hot Stocks"},
    "stock_hk_hot_rank_detail_realtime_em": {"func": get_stock_hk_hot_rank_detail_realtime_em, "desc": "HK hot realtime", "category": "HK Hot Stocks"},
    "stock_hk_hot_rank_latest_em": {"func": get_stock_hk_hot_rank_latest_em, "desc": "HK hot latest", "category": "HK Hot Stocks"},

    # XueQiu Hot
    "stock_hot_deal_xq": {"func": get_stock_hot_deal_xq, "desc": "Hot deal (XueQiu)", "category": "XueQiu Hot"},
    "stock_hot_follow_xq": {"func": get_stock_hot_follow_xq, "desc": "Hot follow (XueQiu)", "category": "XueQiu Hot"},
    "stock_hot_tweet_xq": {"func": get_stock_hot_tweet_xq, "desc": "Hot tweet (XueQiu)", "category": "XueQiu Hot"},
    "stock_inner_trade_xq": {"func": get_stock_inner_trade_xq, "desc": "Inner trade (XueQiu)", "category": "XueQiu Hot"},

    # News & Announcements
    "stock_news_em": {"func": get_stock_news_em, "desc": "Stock news (EastMoney)", "category": "News"},
    "stock_news_main_cx": {"func": get_stock_news_main_cx, "desc": "Main news (CaiXin)", "category": "News"},
    "stock_notice_report": {"func": get_stock_notice_report, "desc": "Notice report", "category": "News"},
    "stock_report_disclosure": {"func": get_stock_report_disclosure, "desc": "Report disclosure", "category": "News"},

    # Comments & Sentiment
    "stock_comment_em": {"func": get_stock_comment_em, "desc": "Stock comment", "category": "Comments"},
    "stock_comment_detail_scrd_desire_em": {"func": get_stock_comment_detail_scrd_desire_em, "desc": "Comment desire detail", "category": "Comments"},
    "stock_comment_detail_scrd_desire_daily_em": {"func": get_stock_comment_detail_scrd_desire_daily_em, "desc": "Comment desire daily", "category": "Comments"},
    "stock_comment_detail_scrd_focus_em": {"func": get_stock_comment_detail_scrd_focus_em, "desc": "Comment focus detail", "category": "Comments"},
    "stock_comment_detail_zhpj_lspf_em": {"func": get_stock_comment_detail_zhpj_lspf_em, "desc": "Comment history rating", "category": "Comments"},
    "stock_comment_detail_zlkp_jgcyd_em": {"func": get_stock_comment_detail_zlkp_jgcyd_em, "desc": "Institutional participation", "category": "Comments"},

    # Weibo Sentiment
    "stock_js_weibo_report": {"func": get_stock_js_weibo_report, "desc": "Weibo stock report", "category": "Weibo Sentiment"},
    "stock_js_weibo_nlp_time": {"func": get_stock_js_weibo_nlp_time, "desc": "Weibo NLP time", "category": "Weibo Sentiment"},

    # ESG Data
    "stock_esg_rate_sina": {"func": get_stock_esg_rate_sina, "desc": "ESG rating (Sina)", "category": "ESG"},
    "stock_esg_hz_sina": {"func": get_stock_esg_hz_sina, "desc": "ESG HZ data", "category": "ESG"},
    "stock_esg_msci_sina": {"func": get_stock_esg_msci_sina, "desc": "MSCI ESG", "category": "ESG"},
    "stock_esg_rft_sina": {"func": get_stock_esg_rft_sina, "desc": "ESG RFT data", "category": "ESG"},
    "stock_esg_zd_sina": {"func": get_stock_esg_zd_sina, "desc": "ESG ZD data", "category": "ESG"},

    # IRM
    "stock_irm_cninfo": {"func": get_stock_irm_cninfo, "desc": "Investor relations (CNINFO)", "category": "Investor Relations"},
    "stock_irm_ans_cninfo": {"func": get_stock_irm_ans_cninfo, "desc": "IRM answers (CNINFO)", "category": "Investor Relations"},

    # SNS Info
    "stock_sns_sseinfo": {"func": get_stock_sns_sseinfo, "desc": "SNS info (SSE)", "category": "SNS Info"},

    # Disclosure
    "stock_zh_a_disclosure_relation_cninfo": {"func": get_stock_zh_a_disclosure_relation_cninfo, "desc": "Disclosure relation", "category": "Disclosure"},
    "stock_zh_a_disclosure_report_cninfo": {"func": get_stock_zh_a_disclosure_report_cninfo, "desc": "Disclosure report", "category": "Disclosure"},

    # Special Data
    "stock_yysj_em": {"func": get_stock_yysj_em, "desc": "Appointment date", "category": "Special Data"},
    "stock_yzxdr_em": {"func": get_stock_yzxdr_em, "desc": "Major event", "category": "Special Data"},
    "stock_zdhtmx_em": {"func": get_stock_zdhtmx_em, "desc": "Major contract", "category": "Special Data"},
    "stock_zygc_em": {"func": get_stock_zygc_em, "desc": "Main business", "category": "Special Data"},
    "stock_gsrl_gsdt_em": {"func": get_stock_gsrl_gsdt_em, "desc": "Company schedule", "category": "Special Data"},
    "stock_info_cjzc_em": {"func": get_stock_info_cjzc_em, "desc": "New stock info", "category": "Special Data"},
    "stock_changes_em": {"func": get_stock_changes_em, "desc": "Stock changes", "category": "Special Data"},
    "stock_qsjy_em": {"func": get_stock_qsjy_em, "desc": "Warrant data", "category": "Special Data"},
    "stock_qbzf_em": {"func": get_stock_qbzf_em, "desc": "Private placement", "category": "Special Data"},
    "stock_tfp_em": {"func": get_stock_tfp_em, "desc": "Suspension data", "category": "Special Data"},
    "stock_sy_em": {"func": get_stock_sy_em, "desc": "Stocks by province", "category": "Special Data"},
    "stock_sy_hy_em": {"func": get_stock_sy_hy_em, "desc": "Stocks by industry", "category": "Special Data"},
    "stock_sy_jz_em": {"func": get_stock_sy_jz_em, "desc": "Stocks by fund", "category": "Special Data"},
    "stock_sy_profile_em": {"func": get_stock_sy_profile_em, "desc": "Stock profile", "category": "Special Data"},
    "stock_sy_yq_em": {"func": get_stock_sy_yq_em, "desc": "Stock forecast", "category": "Special Data"},

    # Vote Data
    "stock_zh_vote_baidu": {"func": get_stock_zh_vote_baidu, "desc": "Stock vote (Baidu)", "category": "Vote"},

    # Management Change
    "stock_management_change_ths": {"func": get_stock_management_change_ths, "desc": "Management change (THS)", "category": "Management"},
}


def get_all_endpoints():
    """Return all available endpoints with descriptions"""
    endpoints = list(ENDPOINTS.keys())
    categories = {}
    for name, info in ENDPOINTS.items():
        cat = info.get("category", "Other")
        if cat not in categories:
            categories[cat] = []
        categories[cat].append(name)

    return {
        "success": True,
        "data": {
            "available_endpoints": endpoints,
            "total_count": len(endpoints),
            "categories": categories
        },
        "timestamp": int(time.time())
    }


def main():
    # Set stdout encoding to UTF-8
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "No endpoint specified", "data": []}))
        sys.exit(1)

    endpoint = sys.argv[1]
    args = sys.argv[2:] if len(sys.argv) > 2 else []

    if endpoint == "get_all_endpoints":
        result = get_all_endpoints()
    elif endpoint in ENDPOINTS:
        func = ENDPOINTS[endpoint]["func"]
        if args:
            result = func(*args)
        else:
            result = func()
    else:
        result = {"success": False, "error": f"Unknown endpoint: {endpoint}", "data": []}

    result["timestamp"] = int(time.time())
    print(json.dumps(result, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()
