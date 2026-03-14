#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AKShare Miscellaneous Functions Wrapper
Covers 129 remaining functions from akshare library (version 1.18.20)
Categories: get, spot, amac, stock, article, fx, air, car, sw, fund, movie, qdii, fred, migration, nlp, qhkc, repo, rv, sunrise, video, and others
"""

import akshare as ak
import json
import sys
from typing import Dict, Any, Optional
from datetime import datetime, date
import pandas as pd


class DateTimeEncoder(json.JSONEncoder):
    """Custom JSON encoder for datetime objects"""
    def default(self, obj):
        if isinstance(obj, (datetime, date)):
            return obj.isoformat()
        if isinstance(obj, pd.Timestamp):
            return obj.isoformat()
        return super().default(obj)


class AKShareError(Exception):
    """Custom exception for AKShare errors"""
    pass


class MiscWrapper:
    """Wrapper for AKShare miscellaneous functions"""

    def __init__(self):
        self.version = "1.18.20"

    def _safe_call_with_retry(self, func, *args, max_retries: int = 3, **kwargs):
        """Safely call akshare function with retry logic"""
        for attempt in range(max_retries):
            try:
                df = func(*args, **kwargs)
                if df is None or (isinstance(df, pd.DataFrame) and df.empty):
                    return {"success": False, "error": "No data returned", "data": None}

                if isinstance(df, pd.DataFrame):
                    for col in df.columns:
                        if pd.api.types.is_datetime64_any_dtype(df[col]):
                            df[col] = df[col].astype(str)
                    # Replace NaN/Infinity with None for valid JSON
                    df = df.replace([float('inf'), float('-inf')], None)
                    df = df.where(pd.notna(df), None)
                    data = df.to_dict(orient='records')
                else:
                    data = df

                return {"success": True, "data": data, "error": None}

            except Exception as e:
                if attempt == max_retries - 1:
                    return {"success": False, "error": str(e), "data": None}
                continue

        return {"success": False, "error": "Max retries exceeded", "data": None}

    # ==================== GET FUNCTIONS (23) ====================

    def get_cffex_daily(self, date: str) -> Dict[str, Any]:
        """Get CFFEX daily data"""
        return self._safe_call_with_retry(ak.get_cffex_daily, date=date)

    def get_cffex_rank_table(self, date: str, symbol: str) -> Dict[str, Any]:
        """Get CFFEX rank table"""
        return self._safe_call_with_retry(ak.get_cffex_rank_table, date=date, symbol=symbol)

    def get_czce_daily(self, date: str) -> Dict[str, Any]:
        """Get CZCE daily data"""
        return self._safe_call_with_retry(ak.get_czce_daily, date=date)

    def get_dce_daily(self, date: str) -> Dict[str, Any]:
        """Get DCE daily data"""
        return self._safe_call_with_retry(ak.get_dce_daily, date=date)

    def get_dce_rank_table(self, date: str, symbol: str) -> Dict[str, Any]:
        """Get DCE rank table"""
        return self._safe_call_with_retry(ak.get_dce_rank_table, date=date, symbol=symbol)

    def get_futures_daily(self, start_date: str, end_date: str, market: str, symbol: str) -> Dict[str, Any]:
        """Get futures daily data"""
        return self._safe_call_with_retry(ak.get_futures_daily, start_date=start_date, end_date=end_date, market=market, symbol=symbol)

    def get_gfex_daily(self, date: str) -> Dict[str, Any]:
        """Get GFEX daily data"""
        return self._safe_call_with_retry(ak.get_gfex_daily, date=date)

    def get_ine_daily(self, date: str) -> Dict[str, Any]:
        """Get INE daily data"""
        return self._safe_call_with_retry(ak.get_ine_daily, date=date)

    def get_qhkc_fund_bs(self) -> Dict[str, Any]:
        """Get QHKC fund balance sheet"""
        return self._safe_call_with_retry(ak.get_qhkc_fund_bs)

    def get_qhkc_fund_money_change(self) -> Dict[str, Any]:
        """Get QHKC fund money change"""
        return self._safe_call_with_retry(ak.get_qhkc_fund_money_change)

    def get_qhkc_fund_position(self) -> Dict[str, Any]:
        """Get QHKC fund position"""
        return self._safe_call_with_retry(ak.get_qhkc_fund_position)

    def get_qhkc_index(self) -> Dict[str, Any]:
        """Get QHKC index"""
        return self._safe_call_with_retry(ak.get_qhkc_index)

    def get_qhkc_index_profit_loss(self) -> Dict[str, Any]:
        """Get QHKC index profit loss"""
        return self._safe_call_with_retry(ak.get_qhkc_index_profit_loss)

    def get_qhkc_index_trend(self) -> Dict[str, Any]:
        """Get QHKC index trend"""
        return self._safe_call_with_retry(ak.get_qhkc_index_trend)

    def get_rank_sum(self, start_date: str, end_date: str, symbol: str) -> Dict[str, Any]:
        """Get rank sum"""
        return self._safe_call_with_retry(ak.get_rank_sum, start_date=start_date, end_date=end_date, symbol=symbol)

    def get_rank_sum_daily(self, date: str, symbol: str) -> Dict[str, Any]:
        """Get daily rank sum"""
        return self._safe_call_with_retry(ak.get_rank_sum_daily, date=date, symbol=symbol)

    def get_rank_table_czce(self, date: str, symbol: str) -> Dict[str, Any]:
        """Get CZCE rank table"""
        return self._safe_call_with_retry(ak.get_rank_table_czce, date=date, symbol=symbol)

    def get_receipt(self, start_date: str, end_date: str, symbol: str) -> Dict[str, Any]:
        """Get receipt data"""
        return self._safe_call_with_retry(ak.get_receipt, start_date=start_date, end_date=end_date, symbol=symbol)

    def get_roll_yield(self, symbol: str, start_date: str, end_date: str, plot: bool = False) -> Dict[str, Any]:
        """Get roll yield"""
        return self._safe_call_with_retry(ak.get_roll_yield, symbol=symbol, start_date=start_date, end_date=end_date, plot=plot)

    def get_roll_yield_bar(self, date: str, plot: bool = False, symbol: str = "all") -> Dict[str, Any]:
        """Get roll yield bar"""
        return self._safe_call_with_retry(ak.get_roll_yield_bar, date=date, plot=plot, symbol=symbol)

    def get_shfe_daily(self, date: str) -> Dict[str, Any]:
        """Get SHFE daily data"""
        return self._safe_call_with_retry(ak.get_shfe_daily, date=date)

    def get_shfe_rank_table(self, date: str, symbol: str) -> Dict[str, Any]:
        """Get SHFE rank table"""
        return self._safe_call_with_retry(ak.get_shfe_rank_table, date=date, symbol=symbol)

    def get_token(self) -> Dict[str, Any]:
        """Get token"""
        return self._safe_call_with_retry(ak.get_token)

    # ==================== SPOT FUNCTIONS (16) ====================

    def spot_corn_price_soozhu(self) -> Dict[str, Any]:
        """Spot corn price from soozhu"""
        return self._safe_call_with_retry(ak.spot_corn_price_soozhu)

    def spot_golden_benchmark_sge(self, symbol: str) -> Dict[str, Any]:
        """SGE spot golden benchmark"""
        return self._safe_call_with_retry(ak.spot_golden_benchmark_sge, symbol=symbol)

    def spot_goods(self) -> Dict[str, Any]:
        """Spot goods data"""
        return self._safe_call_with_retry(ak.spot_goods)

    def spot_hist_sge(self, symbol: str, start_date: str) -> Dict[str, Any]:
        """SGE spot historical data"""
        return self._safe_call_with_retry(ak.spot_hist_sge, symbol=symbol, start_date=start_date)

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

    def spot_price_qh(self, symbol: str, start_date: str, end_date: str) -> Dict[str, Any]:
        """Spot price from qihuo"""
        return self._safe_call_with_retry(ak.spot_price_qh, symbol=symbol, start_date=start_date, end_date=end_date)

    def spot_price_table_qh(self) -> Dict[str, Any]:
        """Spot price table from qihuo"""
        return self._safe_call_with_retry(ak.spot_price_table_qh)

    def spot_quotations_sge(self) -> Dict[str, Any]:
        """SGE spot quotations"""
        return self._safe_call_with_retry(ak.spot_quotations_sge)

    def spot_silver_benchmark_sge(self, symbol: str) -> Dict[str, Any]:
        """SGE spot silver benchmark"""
        return self._safe_call_with_retry(ak.spot_silver_benchmark_sge, symbol=symbol)

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

    def stock_add_stock(self, date: str) -> Dict[str, Any]:
        """Stock additional issuance"""
        return self._safe_call_with_retry(ak.stock_add_stock, date=date)

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

    def article_oman_rv(self) -> Dict[str, Any]:
        """Oman realized volatility"""
        return self._safe_call_with_retry(ak.article_oman_rv)

    def article_oman_rv_short(self) -> Dict[str, Any]:
        """Oman realized volatility (short)"""
        return self._safe_call_with_retry(ak.article_oman_rv_short)

    def article_rlab_rv(self) -> Dict[str, Any]:
        """RLAB realized volatility"""
        return self._safe_call_with_retry(ak.article_rlab_rv)

    # ==================== FX FUNCTIONS (5) ====================

    def fx_c_swap_cm(self) -> Dict[str, Any]:
        """FX commodity swap from ChinaMoney"""
        return self._safe_call_with_retry(ak.fx_c_swap_cm)

    def fx_pair_quote(self, symbol: str) -> Dict[str, Any]:
        """FX pair quote"""
        return self._safe_call_with_retry(ak.fx_pair_quote, symbol=symbol)

    def fx_quote_baidu(self, symbol: str, date: str) -> Dict[str, Any]:
        """FX quote from Baidu"""
        return self._safe_call_with_retry(ak.fx_quote_baidu, symbol=symbol, date=date)

    def fx_spot_quote(self, symbol: str) -> Dict[str, Any]:
        """FX spot quote"""
        return self._safe_call_with_retry(ak.fx_spot_quote, symbol=symbol)

    def fx_swap_quote(self, symbol: str) -> Dict[str, Any]:
        """FX swap quote"""
        return self._safe_call_with_retry(ak.fx_swap_quote, symbol=symbol)

    # ==================== AIR QUALITY (4) ====================

    def air_city_table(self) -> Dict[str, Any]:
        """Air quality city table"""
        return self._safe_call_with_retry(ak.air_city_table)

    def air_quality_hist(self, city: str, period: str = "day", start_date: str = "20140101", end_date: str = "20991231") -> Dict[str, Any]:
        """Air quality historical data"""
        return self._safe_call_with_retry(ak.air_quality_hist, city=city, period=period, start_date=start_date, end_date=end_date)

    def air_quality_rank(self) -> Dict[str, Any]:
        """Air quality ranking"""
        return self._safe_call_with_retry(ak.air_quality_rank)

    def air_quality_watch_point(self, city: str) -> Dict[str, Any]:
        """Air quality watch point"""
        return self._safe_call_with_retry(ak.air_quality_watch_point, city=city)

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

    def sw_index_first_info(self, symbol: str = "801001") -> Dict[str, Any]:
        """SW first level index info"""
        return self._safe_call_with_retry(ak.sw_index_first_info, symbol=symbol)

    def sw_index_second_info(self, symbol: str = "801011") -> Dict[str, Any]:
        """SW second level index info"""
        return self._safe_call_with_retry(ak.sw_index_second_info, symbol=symbol)

    def sw_index_third_cons(self, symbol: str = "850111") -> Dict[str, Any]:
        """SW third level index constituents"""
        return self._safe_call_with_retry(ak.sw_index_third_cons, symbol=symbol)

    def sw_index_third_info(self, symbol: str = "850111") -> Dict[str, Any]:
        """SW third level index info"""
        return self._safe_call_with_retry(ak.sw_index_third_info, symbol=symbol)

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

    def movie_boxoffice_cinema_weekly(self) -> Dict[str, Any]:
        """Weekly cinema box office"""
        return self._safe_call_with_retry(ak.movie_boxoffice_cinema_weekly)

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

    def migration_area_baidu(self, area: str, indicator: str, date: str) -> Dict[str, Any]:
        """Baidu migration area data"""
        return self._safe_call_with_retry(ak.migration_area_baidu, area=area, indicator=indicator, date=date)

    def migration_scale_baidu(self, area: str, indicator: str, start_date: str, end_date: str) -> Dict[str, Any]:
        """Baidu migration scale data"""
        return self._safe_call_with_retry(ak.migration_scale_baidu, area=area, indicator=indicator, start_date=start_date, end_date=end_date)

    # ==================== NLP (2) ====================

    def nlp_answer(self, question: str) -> Dict[str, Any]:
        """NLP question answering"""
        return self._safe_call_with_retry(ak.nlp_answer, question=question)

    def nlp_ownthink(self, question: str) -> Dict[str, Any]:
        """NLP OwnThink question answering"""
        return self._safe_call_with_retry(ak.nlp_ownthink, question=question)

    # ==================== QHKC TOOLS (2) ====================

    def qhkc_tool_foreign(self) -> Dict[str, Any]:
        """QHKC foreign exchange tool"""
        return self._safe_call_with_retry(ak.qhkc_tool_foreign)

    def qhkc_tool_gdp(self) -> Dict[str, Any]:
        """QHKC GDP tool"""
        return self._safe_call_with_retry(ak.qhkc_tool_gdp)

    # ==================== REPO (2) ====================

    def repo_rate_hist(self, symbol: str) -> Dict[str, Any]:
        """Repo rate historical data"""
        return self._safe_call_with_retry(ak.repo_rate_hist, symbol=symbol)

    def repo_rate_query(self) -> Dict[str, Any]:
        """Repo rate query"""
        return self._safe_call_with_retry(ak.repo_rate_query)

    # ==================== REALIZED VOLATILITY (2) ====================

    def rv_from_futures_zh_minute_sina(self, symbol: str, period: str) -> Dict[str, Any]:
        """Realized volatility from futures minute data (Sina)"""
        return self._safe_call_with_retry(ak.rv_from_futures_zh_minute_sina, symbol=symbol, period=period)

    def rv_from_stock_zh_a_hist_min_em(self, symbol: str, period: str, adjust: str = "") -> Dict[str, Any]:
        """Realized volatility from A-share minute data (EastMoney)"""
        return self._safe_call_with_retry(ak.rv_from_stock_zh_a_hist_min_em, symbol=symbol, period=period, adjust=adjust)

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

    def hurun_rank(self, symbol: str = "胡润百富榜") -> Dict[str, Any]:
        """Hurun ranking"""
        return self._safe_call_with_retry(ak.hurun_rank, symbol=symbol)

    def match_main_contract(self, symbol: str) -> Dict[str, Any]:
        """Match main contract"""
        return self._safe_call_with_retry(ak.match_main_contract, symbol=symbol)

    def online_value_artist(self) -> Dict[str, Any]:
        """Artist online value"""
        return self._safe_call_with_retry(ak.online_value_artist)

    def pro_api(self, name: str = "", **kwargs) -> Dict[str, Any]:
        """Pro API access"""
        return self._safe_call_with_retry(ak.pro_api, name=name, **kwargs)

    def rate_interbank(self, indicator: str = "Shibor人民币", symbol: str = "1天") -> Dict[str, Any]:
        """Interbank rate"""
        return self._safe_call_with_retry(ak.rate_interbank, indicator=indicator, symbol=symbol)

    def set_token(self, token: str) -> Dict[str, Any]:
        """Set API token"""
        return self._safe_call_with_retry(ak.set_token, token=token)

    def tool_trade_date_hist_sina(self) -> Dict[str, Any]:
        """Trading date history from Sina"""
        return self._safe_call_with_retry(ak.tool_trade_date_hist_sina)

    def volatility_yz_rv(self, symbol: str, start_date: str, end_date: str) -> Dict[str, Any]:
        """Yang-Zhang realized volatility"""
        return self._safe_call_with_retry(ak.volatility_yz_rv, symbol=symbol, start_date=start_date, end_date=end_date)

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


def main():
    """CLI interface for miscellaneous functions wrapper"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python akshare_misc.py <function_name> [args]",
            "categories": {
                "get_functions": 23,
                "spot_functions": 16,
                "amac_functions": 13,
                "stock_functions": 9,
                "article_functions": 5,
                "fx_functions": 5,
                "air_quality": 4,
                "car_market": 4,
                "sw_index": 4,
                "fund_announcements": 3,
                "movie_boxoffice": 3,
                "qdii": 3,
                "fred": 2,
                "migration": 2,
                "nlp": 2,
                "qhkc_tools": 2,
                "repo": 2,
                "realized_volatility": 2,
                "sunrise": 2,
                "video": 2,
                "single_functions": 15
            },
            "total_functions": 122,
            "example": "python akshare_misc.py get_token"
        }, cls=DateTimeEncoder, ensure_ascii=False, indent=2))
        sys.exit(1)

    wrapper = MiscWrapper()
    function_name = sys.argv[1]

    # Handle get_all_endpoints special case
    if function_name == "get_all_endpoints":
        result = wrapper.get_all_available_endpoints()
        print(json.dumps(result, cls=DateTimeEncoder, ensure_ascii=True))
    elif hasattr(wrapper, function_name):
        func = getattr(wrapper, function_name)
        # Parse additional arguments if needed
        args = sys.argv[2:] if len(sys.argv) > 2 else []
        result = func(*args) if args else func()
        print(json.dumps(result, cls=DateTimeEncoder, ensure_ascii=True))
    else:
        print(json.dumps({
            "error": f"Function '{function_name}' not found",
            "hint": "Use 'python akshare_misc.py' to see available functions"
        }, cls=DateTimeEncoder, ensure_ascii=True))
        sys.exit(1)


if __name__ == "__main__":
    main()
