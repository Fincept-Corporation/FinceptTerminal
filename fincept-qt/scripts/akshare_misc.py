#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AKShare Miscellaneous Functions Wrapper
Covers 129 remaining functions from akshare library (version 1.18.20)
Categories: get, spot, amac, stock, article, fx, air, car, sw, fund, movie, qdii, fred, migration, nlp, repo, rv, sunrise, video, and others
"""

import akshare as ak

# safe_call: one akshare call -> {success, data, columns, count} or a plain-English error;
# run_wrapper_cli: get_all_endpoints / <endpoint> [field=value ...] -> one JSON line on stdout.
from akshare_screen_args import china_today, run_wrapper_cli, safe_call
import json
import sys
from typing import Dict, Any, List, Optional
from datetime import datetime, date, time as dt_time, timedelta
import pandas as pd


def _days_back(days: int, start_date: str = "", end_date: str = ""):
    """(start, end) as YYYYMMDD: the dates typed, else the last `days` days up to today."""
    today = china_today()
    return (start_date or (today - timedelta(days=days)).strftime("%Y%m%d"),
            end_date or today.strftime("%Y%m%d"))


def _vars_list(symbol: str) -> Dict[str, Any]:
    """futures ranking functions take vars_list; an empty symbol keeps akshare's all-varieties default."""
    return {"vars_list": [symbol.strip().upper()]} if symbol and symbol.strip() else {}


# The AkShare screen sends its boxes as symbol=/start_date=/end_date=/period=; run_wrapper_cli
# applies each by parameter name (START fills `date` for the single-day endpoints) and answers a
# missing required value with a JSON error naming the box instead of a TypeError traceback.


class MiscWrapper:
    """Wrapper for AKShare miscellaneous functions"""

    def __init__(self):
        self.version = "1.18.20"

    def _safe_call_with_retry(self, func, *args, max_retries: int = 2, **kwargs) -> Dict[str, Any]:
        """One akshare call -> {success, data, columns, count} or a plain-English error (shared
        akshare_screen_args.safe_call: only a dropped connection or a timeout is retried, never a
        rate limit or a refusal)."""
        return safe_call(func, *args, _retries=max_retries, _key_column="contract", **kwargs)

    # ==================== FUTURES EXCHANGE DATA (akshare futures_* functions) ====================
    # Dates are trading days as YYYYMMDD; symbol is a futures variety code (IF, RB, M, ...).

    def get_cffex_daily(self, date: str) -> Dict[str, Any]:
        """Get CFFEX daily data"""
        return self._safe_call_with_retry(ak.futures_daily_cffex, date=date)

    def get_cffex_rank_table(self, date: str, symbol: str = "") -> Dict[str, Any]:
        """CFFEX member position ranking (symbol: variety, empty = all)"""
        return self._safe_call_with_retry(ak.futures_rank_table_cffex, date=date, **_vars_list(symbol))

    def get_czce_daily(self, date: str) -> Dict[str, Any]:
        """Get CZCE daily data"""
        return self._safe_call_with_retry(ak.futures_daily_czce, date=date)

    def get_dce_daily(self, date: str) -> Dict[str, Any]:
        """Get DCE daily data"""
        return self._safe_call_with_retry(ak.futures_daily_dce, date=date)

    def get_dce_rank_table(self, date: str, symbol: str = "") -> Dict[str, Any]:
        """DCE member position ranking (symbol: variety, empty = all)"""
        return self._safe_call_with_retry(ak.futures_rank_table_dce, date=date, **_vars_list(symbol))

    def get_futures_daily(self, start_date: str, end_date: str, market: str = "CFFEX") -> Dict[str, Any]:
        """Daily quotes of every contract on one exchange (CFFEX, SHFE, DCE, CZCE, INE, GFEX)"""
        return self._safe_call_with_retry(ak.futures_daily, start_date=start_date, end_date=end_date, market=market)

    def get_gfex_daily(self, date: str) -> Dict[str, Any]:
        """Get GFEX daily data"""
        return self._safe_call_with_retry(ak.futures_daily_gfex, date=date)

    def get_ine_daily(self, date: str) -> Dict[str, Any]:
        """Get INE daily data"""
        return self._safe_call_with_retry(ak.futures_daily_ine, date=date)


    def get_rank_sum(self, date: str, symbol: str = "") -> Dict[str, Any]:
        """Top-20 member position totals for one trading day (symbol: variety, empty = all)"""
        return self._safe_call_with_retry(ak.futures_rank_sum, date=date, **_vars_list(symbol))

    def get_rank_sum_daily(self, start_date: str, end_date: str, symbol: str = "") -> Dict[str, Any]:
        """Top-20 member position totals per day over a date range (symbol: variety, empty = all)"""
        return self._safe_call_with_retry(ak.futures_rank_sum_daily, start_day=start_date, end_day=end_date,
                                          **_vars_list(symbol))

    def get_rank_table_czce(self, date: str) -> Dict[str, Any]:
        """CZCE member position ranking, all contracts"""
        return self._safe_call_with_retry(ak.futures_rank_table_czce, date=date)

    def get_receipt(self, start_date: str, end_date: str, symbol: str = "") -> Dict[str, Any]:
        """Warehouse receipts over a date range (symbol: variety, empty = all)"""
        return self._safe_call_with_retry(ak.futures_receipt, start_date=start_date, end_date=end_date,
                                          **_vars_list(symbol))

    def get_roll_yield(self, symbol: str, date: str) -> Dict[str, Any]:
        """Roll yield of a variety's main / next contracts on one trading day"""
        return self._safe_call_with_retry(ak.futures_roll_yield, date=date, var=symbol.upper())

    def get_roll_yield_bar(self, symbol: str, date: str) -> Dict[str, Any]:
        """Roll yield across a variety's contracts on one trading day"""
        return self._safe_call_with_retry(ak.futures_roll_yield_bar, type_method="var", var=symbol.upper(), date=date)

    def get_shfe_daily(self, date: str) -> Dict[str, Any]:
        """Get SHFE daily data"""
        return self._safe_call_with_retry(ak.futures_daily_shfe, date=date)

    def get_shfe_rank_table(self, date: str, symbol: str = "") -> Dict[str, Any]:
        """SHFE member position ranking (symbol: variety, empty = all)"""
        return self._safe_call_with_retry(ak.futures_rank_table_shfe, date=date, **_vars_list(symbol))


    # ==================== SPOT FUNCTIONS (16) ====================

    def spot_corn_price_soozhu(self) -> Dict[str, Any]:
        """Spot corn price from soozhu"""
        return self._safe_call_with_retry(ak.spot_corn_price_soozhu)

    def spot_golden_benchmark_sge(self) -> Dict[str, Any]:
        """SGE spot golden benchmark"""
        return self._safe_call_with_retry(ak.spot_golden_benchmark_sge)

    def spot_goods(self) -> Dict[str, Any]:
        """Spot goods data"""
        return self._safe_call_with_retry(ak.spot_goods)

    def spot_hist_sge(self, symbol: str = "Au99.99") -> Dict[str, Any]:
        """SGE spot historical data (symbol e.g. Au99.99, Ag99.99)"""
        return self._safe_call_with_retry(ak.spot_hist_sge, symbol=symbol)

    def spot_hog_crossbred_soozhu(self) -> Dict[str, Any]:
        """Spot crossbred hog price from soozhu"""
        return self._safe_call_with_retry(ak.spot_hog_crossbred_soozhu)

    def spot_hog_lean_price_soozhu(self) -> Dict[str, Any]:
        """Spot lean hog price from soozhu"""
        return self._safe_call_with_retry(ak.spot_hog_lean_price_soozhu)

    def spot_hog_soozhu(self) -> Dict[str, Any]:
        """Spot hog price from soozhu"""
        return self._safe_call_with_retry(ak.spot_hog_soozhu)

    def spot_hog_three_way_soozhu(self) -> Dict[str, Any]:
        """Spot three-way hog price from soozhu"""
        return self._safe_call_with_retry(ak.spot_hog_three_way_soozhu)

    def spot_hog_year_trend_soozhu(self) -> Dict[str, Any]:
        """Spot hog yearly trend from soozhu"""
        return self._safe_call_with_retry(ak.spot_hog_year_trend_soozhu)

    def spot_mixed_feed_soozhu(self) -> Dict[str, Any]:
        """Spot mixed feed price from soozhu"""
        return self._safe_call_with_retry(ak.spot_mixed_feed_soozhu)

    def spot_price_qh(self, symbol: str = "螺纹钢") -> Dict[str, Any]:
        """Spot price history from qihuo (symbol: commodity name, e.g. 螺纹钢)"""
        return self._safe_call_with_retry(ak.spot_price_qh, symbol=symbol)

    def spot_price_table_qh(self) -> Dict[str, Any]:
        """Spot price table from qihuo"""
        return self._safe_call_with_retry(ak.spot_price_table_qh)

    def spot_quotations_sge(self) -> Dict[str, Any]:
        """SGE spot quotations"""
        return self._safe_call_with_retry(ak.spot_quotations_sge)

    def spot_silver_benchmark_sge(self) -> Dict[str, Any]:
        """SGE spot silver benchmark"""
        return self._safe_call_with_retry(ak.spot_silver_benchmark_sge)

    def spot_soybean_price_soozhu(self) -> Dict[str, Any]:
        """Spot soybean price from soozhu"""
        return self._safe_call_with_retry(ak.spot_soybean_price_soozhu)

    def spot_symbol_table_sge(self) -> Dict[str, Any]:
        """SGE spot symbol table"""
        return self._safe_call_with_retry(ak.spot_symbol_table_sge)

    # ==================== AMAC FUNCTIONS (13) ====================

    def amac_aoin_info(self) -> Dict[str, Any]:
        """AMAC AOIN information"""
        return self._safe_call_with_retry(ak.amac_aoin_info)

    def amac_fund_abs(self) -> Dict[str, Any]:
        """AMAC fund ABS"""
        return self._safe_call_with_retry(ak.amac_fund_abs)

    def amac_fund_account_info(self) -> Dict[str, Any]:
        """AMAC fund account information"""
        return self._safe_call_with_retry(ak.amac_fund_account_info)

    def amac_fund_info(self) -> Dict[str, Any]:
        """AMAC fund information"""
        return self._safe_call_with_retry(ak.amac_fund_info)

    def amac_fund_sub_info(self) -> Dict[str, Any]:
        """AMAC fund sub information"""
        return self._safe_call_with_retry(ak.amac_fund_sub_info)

    def amac_futures_info(self) -> Dict[str, Any]:
        """AMAC futures information"""
        return self._safe_call_with_retry(ak.amac_futures_info)

    def amac_manager_cancelled_info(self) -> Dict[str, Any]:
        """AMAC manager cancelled information"""
        return self._safe_call_with_retry(ak.amac_manager_cancelled_info)

    def amac_manager_classify_info(self) -> Dict[str, Any]:
        """AMAC manager classification information"""
        return self._safe_call_with_retry(ak.amac_manager_classify_info)

    def amac_manager_info(self) -> Dict[str, Any]:
        """AMAC manager information"""
        return self._safe_call_with_retry(ak.amac_manager_info)

    def amac_member_info(self) -> Dict[str, Any]:
        """AMAC member information"""
        return self._safe_call_with_retry(ak.amac_member_info)

    def amac_member_sub_info(self) -> Dict[str, Any]:
        """AMAC member sub information"""
        return self._safe_call_with_retry(ak.amac_member_sub_info)

    def amac_person_fund_org_list(self) -> Dict[str, Any]:
        """AMAC person fund organization list"""
        return self._safe_call_with_retry(ak.amac_person_fund_org_list)

    def amac_securities_info(self) -> Dict[str, Any]:
        """AMAC securities information"""
        return self._safe_call_with_retry(ak.amac_securities_info)

    # ==================== STOCK FUNCTIONS (9) ====================

    def stock_account_statistics_em(self) -> Dict[str, Any]:
        """Stock account statistics from EastMoney"""
        return self._safe_call_with_retry(ak.stock_account_statistics_em)

    def stock_add_stock(self, symbol: str) -> Dict[str, Any]:
        """Stock additional issuance (symbol: 6-digit code)"""
        return self._safe_call_with_retry(ak.stock_add_stock, symbol=symbol)

    def stock_financial_abstract_new_ths(self, symbol: str, indicator: str = "按报告期") -> Dict[str, Any]:
        """THS stock financial abstract (new)"""
        return self._safe_call_with_retry(ak.stock_financial_abstract_new_ths, symbol=symbol, indicator=indicator)

    def stock_financial_benefit_new_ths(self, symbol: str, indicator: str = "按报告期") -> Dict[str, Any]:
        """THS stock financial benefit (new)"""
        return self._safe_call_with_retry(ak.stock_financial_benefit_new_ths, symbol=symbol, indicator=indicator)

    def stock_financial_cash_new_ths(self, symbol: str, indicator: str = "按报告期") -> Dict[str, Any]:
        """THS stock financial cash flow (new)"""
        return self._safe_call_with_retry(ak.stock_financial_cash_new_ths, symbol=symbol, indicator=indicator)

    def stock_financial_debt_new_ths(self, symbol: str, indicator: str = "按报告期") -> Dict[str, Any]:
        """THS stock financial debt (new)"""
        return self._safe_call_with_retry(ak.stock_financial_debt_new_ths, symbol=symbol, indicator=indicator)

    def stock_register_all_em(self) -> Dict[str, Any]:
        """Stock register all from EastMoney"""
        return self._safe_call_with_retry(ak.stock_register_all_em)

    def stock_us_valuation_baidu(self, symbol: str) -> Dict[str, Any]:
        """US stock valuation from Baidu"""
        return self._safe_call_with_retry(ak.stock_us_valuation_baidu, symbol=symbol)

    def stock_zyjs_ths(self, symbol: str) -> Dict[str, Any]:
        """THS stock main business"""
        return self._safe_call_with_retry(ak.stock_zyjs_ths, symbol=symbol)

    # ==================== ARTICLE FUNCTIONS (5) ====================

    def article_epu_index(self) -> Dict[str, Any]:
        """Economic Policy Uncertainty index"""
        return self._safe_call_with_retry(ak.article_epu_index)

    def article_ff_crr(self) -> Dict[str, Any]:
        """Fama-French CRR factors"""
        return self._safe_call_with_retry(ak.article_ff_crr)


    def article_rlab_rv(self) -> Dict[str, Any]:
        """RLAB realized volatility"""
        return self._safe_call_with_retry(ak.article_rlab_rv)

    # ==================== FX FUNCTIONS (5) ====================

    def fx_c_swap_cm(self) -> Dict[str, Any]:
        """FX commodity swap from ChinaMoney"""
        return self._safe_call_with_retry(ak.fx_c_swap_cm)

    def fx_pair_quote(self) -> Dict[str, Any]:
        """FX pair quote"""
        return self._safe_call_with_retry(ak.fx_pair_quote)

    def fx_quote_baidu(self, symbol: str = "人民币") -> Dict[str, Any]:
        """FX quotes against one currency from Baidu (symbol: currency name, e.g. 人民币, 美元)"""
        return self._safe_call_with_retry(ak.fx_quote_baidu, symbol=symbol)

    def fx_spot_quote(self) -> Dict[str, Any]:
        """FX spot quote"""
        return self._safe_call_with_retry(ak.fx_spot_quote)

    def fx_swap_quote(self) -> Dict[str, Any]:
        """FX swap quote"""
        return self._safe_call_with_retry(ak.fx_swap_quote)

    # ==================== AIR QUALITY (4) ====================

    def air_city_table(self) -> Dict[str, Any]:
        """Air quality city table"""
        return self._safe_call_with_retry(ak.air_city_table)

    # The screen has no box for a city, area or question: these endpoints open a useful default
    # (Beijing, the last weeks) and a manual run can name another (air_quality_hist 上海 day ...).

    def air_quality_hist(self, city: str = "北京", period: str = "day", start_date: str = "",
                         end_date: str = "") -> Dict[str, Any]:
        """Daily air quality of one city (Chinese name); default the last 30 days"""
        start, end = _days_back(30, start_date, end_date)
        return self._safe_call_with_retry(ak.air_quality_hist, city=city, period=period, start_date=start, end_date=end)

    def air_quality_rank(self) -> Dict[str, Any]:
        """Air quality ranking"""
        return self._safe_call_with_retry(ak.air_quality_rank)

    def air_quality_watch_point(self, city: str = "北京", start_date: str = "", end_date: str = "") -> Dict[str, Any]:
        """Air quality per monitoring station of one city; default the last 7 days"""
        start, end = _days_back(7, start_date, end_date)
        return self._safe_call_with_retry(ak.air_quality_watch_point, city=city, start_date=start, end_date=end)

    # ==================== CAR MARKET (4) ====================

    def car_market_cate_cpca(self) -> Dict[str, Any]:
        """Car market category from CPCA"""
        return self._safe_call_with_retry(ak.car_market_cate_cpca)

    def car_market_country_cpca(self) -> Dict[str, Any]:
        """Car market country from CPCA"""
        return self._safe_call_with_retry(ak.car_market_country_cpca)

    def car_market_fuel_cpca(self) -> Dict[str, Any]:
        """Car market fuel type from CPCA"""
        return self._safe_call_with_retry(ak.car_market_fuel_cpca)

    def car_market_man_rank_cpca(self) -> Dict[str, Any]:
        """Car market manufacturer ranking from CPCA"""
        return self._safe_call_with_retry(ak.car_market_man_rank_cpca)

    # ==================== SW INDEX (4) ====================

    def sw_index_first_info(self) -> Dict[str, Any]:
        """SW first level index info"""
        return self._safe_call_with_retry(ak.sw_index_first_info)

    def sw_index_second_info(self) -> Dict[str, Any]:
        """SW second level index info"""
        return self._safe_call_with_retry(ak.sw_index_second_info)

    def sw_index_third_cons(self, symbol: str = "850111") -> Dict[str, Any]:
        """SW third level index constituents"""
        return self._safe_call_with_retry(ak.sw_index_third_cons, symbol=symbol)

    def sw_index_third_info(self) -> Dict[str, Any]:
        """SW third level index info"""
        return self._safe_call_with_retry(ak.sw_index_third_info)

    # ==================== FUND ANNOUNCEMENTS (3) ====================

    def fund_announcement_dividend_em(self, symbol: str) -> Dict[str, Any]:
        """Fund dividend announcement from EastMoney"""
        return self._safe_call_with_retry(ak.fund_announcement_dividend_em, symbol=symbol)

    def fund_announcement_personnel_em(self, symbol: str) -> Dict[str, Any]:
        """Fund personnel announcement from EastMoney"""
        return self._safe_call_with_retry(ak.fund_announcement_personnel_em, symbol=symbol)

    def fund_announcement_report_em(self, symbol: str) -> Dict[str, Any]:
        """Fund report announcement from EastMoney"""
        return self._safe_call_with_retry(ak.fund_announcement_report_em, symbol=symbol)

    # ==================== MOVIE BOX OFFICE (3) ====================

    def movie_boxoffice_cinema_daily(self, date: str) -> Dict[str, Any]:
        """Daily cinema box office"""
        return self._safe_call_with_retry(ak.movie_boxoffice_cinema_daily, date=date)


    def movie_boxoffice_yearly_first_week(self) -> Dict[str, Any]:
        """Yearly first week box office"""
        return self._safe_call_with_retry(ak.movie_boxoffice_yearly_first_week)

    # ==================== QDII (3) ====================

    def qdii_a_index_jsl(self) -> Dict[str, Any]:
        """QDII A-share index from JiSiLu"""
        return self._safe_call_with_retry(ak.qdii_a_index_jsl)

    def qdii_e_comm_jsl(self) -> Dict[str, Any]:
        """QDII commodity from JiSiLu"""
        return self._safe_call_with_retry(ak.qdii_e_comm_jsl)

    def qdii_e_index_jsl(self) -> Dict[str, Any]:
        """QDII equity index from JiSiLu"""
        return self._safe_call_with_retry(ak.qdii_e_index_jsl)

    # ==================== FRED (2) ====================

    def fred_md(self) -> Dict[str, Any]:
        """FRED monthly database"""
        return self._safe_call_with_retry(ak.fred_md)

    def fred_qd(self) -> Dict[str, Any]:
        """FRED quarterly database"""
        return self._safe_call_with_retry(ak.fred_qd)

    # ==================== MIGRATION (2) ====================

    def migration_area_baidu(self, area: str = "北京市", indicator: str = "move_in", date: str = "") -> Dict[str, Any]:
        """Baidu migration sources (move_in) or destinations (move_out) of one area, one day (default yesterday)"""
        day = date or (china_today() - timedelta(days=1)).strftime("%Y%m%d")
        return self._safe_call_with_retry(ak.migration_area_baidu, area=area, indicator=indicator, date=day)

    def migration_scale_baidu(self, area: str = "广州市", indicator: str = "move_in") -> Dict[str, Any]:
        """Baidu migration scale index for a city (indicator: move_in / move_out)"""
        return self._safe_call_with_retry(ak.migration_scale_baidu, area=area, indicator=indicator)

    # ==================== NLP (2) ====================

    def nlp_answer(self, symbol: str = "人工智能") -> Dict[str, Any]:
        """Knowledge-base answer (ownthink) to the question typed in the SYMBOL box"""
        return self._safe_call_with_retry(ak.nlp_answer, question=symbol)


    # ==================== QHKC TOOLS (2) ====================


    # ==================== REPO (2) ====================

    def repo_rate_hist(self, start_date: str, end_date: str) -> Dict[str, Any]:
        """Repo fixing rates over a date range (YYYYMMDD)"""
        return self._safe_call_with_retry(ak.repo_rate_hist, start_date=start_date, end_date=end_date)

    def repo_rate_query(self) -> Dict[str, Any]:
        """Repo rate query"""
        return self._safe_call_with_retry(ak.repo_rate_query)

    # ==================== REALIZED VOLATILITY (2) ====================

    # Realized volatility from minute bars; `period` is the bar length in minutes ("1", "5", ...), which
    # the screen's daily/weekly/monthly PERIOD box does not set.

    def rv_from_futures_zh_minute_sina(self, symbol: str = "RB0", period: str = "5") -> Dict[str, Any]:
        """Realized volatility of one futures contract (Sina; RB0 = rebar main contract)"""
        return self._safe_call_with_retry(ak.rv_from_futures_zh_minute_sina, symbol=symbol, period=period)

    def rv_from_stock_zh_a_hist_min_em(self, symbol: str = "000001", period: str = "5", adjust: str = "",
                                       start_date: str = "", end_date: str = "") -> Dict[str, Any]:
        """Realized volatility of one A-share from minute bars (EastMoney); default the last 5 days"""
        start, end = _days_back(5, start_date, end_date)
        return self._safe_call_with_retry(ak.rv_from_stock_zh_a_hist_min_em, symbol=symbol, period=period,
                                          adjust=adjust, start_date=f"{start[:4]}-{start[4:6]}-{start[6:8]} 09:30:00",
                                          end_date=f"{end[:4]}-{end[4:6]}-{end[6:8]} 15:00:00")

    # ==================== SUNRISE (2) ====================

    def sunrise_daily(self) -> Dict[str, Any]:
        """Daily sunrise/sunset time"""
        return self._safe_call_with_retry(ak.sunrise_daily)

    def sunrise_monthly(self) -> Dict[str, Any]:
        """Monthly sunrise/sunset time"""
        return self._safe_call_with_retry(ak.sunrise_monthly)

    # ==================== VIDEO (2) ====================

    def video_tv(self) -> Dict[str, Any]:
        """TV viewership data"""
        return self._safe_call_with_retry(ak.video_tv)

    def video_variety_show(self) -> Dict[str, Any]:
        """Variety show viewership data"""
        return self._safe_call_with_retry(ak.video_variety_show)

    # ==================== SINGLE FUNCTIONS (15) ====================

    def bank_fjcf_table_detail(self) -> Dict[str, Any]:
        """Bank FJCF table detail"""
        return self._safe_call_with_retry(ak.bank_fjcf_table_detail)

    def business_value_artist(self) -> Dict[str, Any]:
        """Artist business value"""
        return self._safe_call_with_retry(ak.business_value_artist)

    def drewry_wci_index(self) -> Dict[str, Any]:
        """Drewry World Container Index"""
        return self._safe_call_with_retry(ak.drewry_wci_index)

    def forbes_rank(self, symbol: str = "中国富豪榜") -> Dict[str, Any]:
        """Forbes ranking"""
        return self._safe_call_with_retry(ak.forbes_rank, symbol=symbol)

    def forex_hist_em(self, symbol: str) -> Dict[str, Any]:
        """Forex historical data from EastMoney"""
        return self._safe_call_with_retry(ak.forex_hist_em, symbol=symbol)

    def hf_sp_500(self) -> Dict[str, Any]:
        """S&P 500 hedge fund data"""
        return self._safe_call_with_retry(ak.hf_sp_500)

    def hurun_rank(self, indicator: str = "胡润百富榜", year: str = "") -> Dict[str, Any]:
        """Hurun ranking (year empty = akshare's default edition)"""
        return self._safe_call_with_retry(ak.hurun_rank, indicator=indicator, **({"year": year} if year else {}))

    def match_main_contract(self, symbol: str) -> Dict[str, Any]:
        """Match main contract"""
        return self._safe_call_with_retry(ak.match_main_contract, symbol=symbol)

    def online_value_artist(self) -> Dict[str, Any]:
        """Artist online value"""
        return self._safe_call_with_retry(ak.online_value_artist)


    def rate_interbank(self, indicator: str = "Shibor人民币", symbol: str = "1天") -> Dict[str, Any]:
        """Interbank rate"""
        return self._safe_call_with_retry(ak.rate_interbank, indicator=indicator, symbol=symbol)


    def tool_trade_date_hist_sina(self) -> Dict[str, Any]:
        """Trading date history from Sina"""
        return self._safe_call_with_retry(ak.tool_trade_date_hist_sina)


    def xincaifu_rank(self) -> Dict[str, Any]:
        """Xincaifu ranking"""
        return self._safe_call_with_retry(ak.xincaifu_rank)

    def get_all_available_endpoints(self) -> Dict[str, Any]:
        """Get list of all available misc endpoints"""
        endpoints = [name for name in dir(self) if not name.startswith('_') and callable(getattr(self, name)) and name != 'get_all_available_endpoints']
        return {
            "success": True,
            "data": {
                "available_endpoints": endpoints,
                "total_count": len(endpoints),
                "categories": {
                    "Get Functions": [e for e in endpoints if e.startswith('get_')],
                    "Spot Functions": [e for e in endpoints if e.startswith('spot_')],
                    "AMAC": [e for e in endpoints if 'amac' in e],
                    "Stock": [e for e in endpoints if 'stock' in e and not e.startswith('get_')],
                    "Air Quality": [e for e in endpoints if 'air' in e],
                    "Other": [e for e in endpoints if not any(e.startswith(p) for p in ['get_', 'spot_']) and not any(k in e for k in ['amac', 'stock', 'air'])]
                }
            },
            "count": len(endpoints)
        }


# ==================== CLI ====================

def main():
    wrapper = MiscWrapper()
    # One JSON line on stdout (NaN -> null, dates as ISO text); akshare's own prints go to stderr.
    return run_wrapper_cli(wrapper, wrapper.get_all_available_endpoints, ascii=True, iso_dates=True)


if __name__ == "__main__":
    sys.exit(main())
