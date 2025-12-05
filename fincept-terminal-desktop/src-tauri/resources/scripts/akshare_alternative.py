"""
AKShare Alternative Data Wrapper
Comprehensive wrapper for alternative and specialized data sources
Returns JSON output for Rust integration
"""

import sys
import json
import pandas as pd
import akshare as ak
from typing import Dict, Any, List, Optional, Union
from datetime import datetime, timedelta
import time
import traceback


class AKShareError:
    """Custom error class for AKShare API errors"""
    def __init__(self, endpoint: str, error: str, data_source: Optional[str] = None):
        self.endpoint = endpoint
        self.error = error
        self.data_source = data_source
        self.timestamp = int(datetime.now().timestamp())

    def to_dict(self) -> Dict[str, Any]:
        return {
            "endpoint": self.endpoint,
            "error": self.error,
            "data_source": self.data_source,
            "timestamp": self.timestamp,
            "type": "AKShareError"
        }


class AlternativeDataWrapper:
    """Comprehensive alternative data wrapper with 60+ endpoints"""

    def __init__(self):
        self.session = None
        self.default_timeout = 30
        self.retry_delay = 2

        # Common date parameters
        self.default_start_date = (datetime.now() - timedelta(days=365)).strftime('%Y%m%d')
        self.default_end_date = datetime.now().strftime('%Y%m%d')

    def _safe_call_with_retry(self, func, *args, max_retries: int = 3, **kwargs) -> Dict[str, Any]:
        """Safely call AKShare function with enhanced error handling and retry logic"""
        last_error = None

        for attempt in range(max_retries):
            try:
                result = func(*args, **kwargs)

                # Enhanced data validation
                if result is not None:
                    if hasattr(result, 'empty') and not result.empty:
                        return {
                            "success": True,
                            "data": result.to_dict('records'),
                            "count": len(result),
                            "timestamp": int(datetime.now().timestamp()),
                            "data_quality": "high",
                            "source": f"akshare.{func.__name__}"
                        }
                    elif hasattr(result, '__len__') and len(result) > 0:
                        # Handle non-DataFrame results
                        return {
                            "success": True,
                            "data": result if isinstance(result, list) else [result],
                            "count": len(result) if isinstance(result, list) else 1,
                            "timestamp": int(datetime.now().timestamp()),
                            "data_quality": "high",
                            "source": f"akshare.{func.__name__}"
                        }
                    else:
                        return {
                            "success": False,
                            "error": "No data returned",
                            "data": [],
                            "count": 0,
                            "timestamp": int(datetime.now().timestamp()),
                            "data_quality": "low"
                        }

            except Exception as e:
                last_error = str(e)
                if attempt < max_retries - 1:
                    time.sleep(self.retry_delay ** attempt)
                    continue

                error_obj = AKShareError(
                    endpoint=func.__name__,
                    error=last_error,
                    data_source=getattr(func, '__module__', 'unknown')
                )
                return {
                    "success": False,
                    "error": error_obj.to_dict(),
                    "data": [],
                    "count": 0,
                    "timestamp": int(datetime.now().timestamp()),
                    "data_quality": "experimental"
                }

        # Should never reach here
        return {
            "success": False,
            "error": f"Max retries exceeded: {last_error}",
            "data": [],
            "count": 0,
            "timestamp": int(datetime.now().timestamp())
        }

    # ==================== ENVIRONMENTAL DATA ====================

    def get_air_quality_hebei(self) -> Dict[str, Any]:
        """Get Hebei province air quality data"""
        return self._safe_call_with_retry(ak.air_quality_hebei)

    def get_air_zhenqi(self) -> Dict[str, Any]:
        """Get nationwide air quality data"""
        return self._safe_call_with_retry(ak.air_zhenqi)

    def get_air_china_daily(self) -> Dict[str, Any]:
        """Get daily air quality data for China"""
        return self._safe_call_with_retry(ak.air_china_daily)

    def get_air_city_ranking(self) -> Dict[str, Any]:
        """Get air quality city ranking"""
        return self._safe_call_with_retry(ak.air_city_ranking)

    def get_air_pollution_index(self) -> Dict[str, Any]:
        """Get air pollution index"""
        return self._safe_call_with_retry(ak.air_pollution_index)

    def get_energy_carbon(self) -> Dict[str, Any]:
        """Get carbon emission trading data"""
        return self._safe_call_with_retry(ak.energy_carbon)

    def get_energy_carbon_price(self) -> Dict[str, Any]:
        """Get carbon price data"""
        return self._safe_call_with_retry(ak.energy_carbon_price)

    def get_energy_carbon_quota(self) -> Dict[str, Any]:
        """Get carbon quota data"""
        return self._safe_call_with_retry(ak.energy_carbon_quota)

    def get_energy_oil_em(self) -> Dict[str, Any]:
        """Get oil price data"""
        return self._safe_call_with_retry(ak.energy_oil_em)

    def get_energy_oil_ice(self) -> Dict[str, Any]:
        """Get ICE oil price data"""
        return self._safe_call_with_retry(ak.energy_oil_ice)

    def get_energy_natural_gas(self) -> Dict[str, Any]:
        """Get natural gas data"""
        return self._safe_call_with_retry(ak.energy_natural_gas)

    def get_energy_coal(self) -> Dict[str, Any]:
        """Get coal price data"""
        return self._safe_call_with_retry(ak.energy_coal)

    def get_energy_electricity(self) -> Dict[str, Any]:
        """Get electricity price data"""
        return self._safe_call_with_retry(ak.energy_electricity)

    def get_energy_solar(self) -> Dict[str, Any]:
        """Get solar energy data"""
        return self._safe_call_with_retry(ak.energy_solar)

    def get_energy_wind(self) -> Dict[str, Any]:
        """Get wind energy data"""
        return self._safe_call_with_retry(ak.energy_wind)

    def get_energy_hydro(self) -> Dict[str, Any]:
        """Get hydroelectric data"""
        return self._safe_call_with_retry(ak.energy_hydro)

    # ==================== CONSUMER DATA ====================

    def get_cost_living(self) -> Dict[str, Any]:
        """Get cost of living indices"""
        return self._safe_call_with_retry(ak.cost_living)

    def get_cost_living_city(self) -> Dict[str, Any]:
        """Get cost of living by city"""
        return self._safe_call_with_retry(ak.cost_living_city)

    def get_cost_living_country(self) -> Dict[str, Any]:
        """Get cost of living by country"""
        return self._safe_call_with_retry(ak.cost_living_country)

    def get_movie_yien(self) -> Dict[str, Any]:
        """Get movie box office data"""
        return self._safe_call_with_retry(ak.movie_yien)

    def get_movie_boxoffice(self) -> Dict[str, Any]:
        """Get movie box office data"""
        return self._safe_call_with_retry(ak.movie_boxoffice)

    def get_movie_maoyan(self) -> Dict[str, Any]:
        """Get Maoyan movie data"""
        return self._safe_call_with_retry(ak.movie_maoyan)

    def get_movie_taopiaopiao(self) -> Dict[str, Any]:
        """Get Taopiaopiao movie data"""
        return self._safe_call_with_retry(ak.movie_taopiaopiao)

    def get_movie_douban(self) -> Dict[str, Any]:
        """Get Douban movie ratings"""
        return self._safe_call_with_retry(ak.movie_douban)

    def get_movie_ranking(self) -> Dict[str, Any]:
        """Get movie ranking data"""
        return self._safe_call_with_retry(ak.movie_ranking)

    def get_music_qq(self) -> Dict[str, Any]:
        """Get QQ music charts"""
        return self._safe_call_with_retry(ak.music_qq)

    def get_music_netease(self) -> Dict[str, Any]:
        """Get Netease music data"""
        return self._safe_call_with_retry(ak.music_netease)

    def get_music_kugou(self) -> Dict[str, Any]:
        """Get Kugou music data"""
        return self._safe_call_with_retry(ak.music_kugou)

    def get_music_kuwo(self) -> Dict[str, Any]:
        """Get Kuwo music data"""
        return self._safe_call_with_retry(ak.music_kuwo)

    def get_gaming_tencent(self) -> Dict[str, Any]:
        """Get Tencent gaming data"""
        return self._safe_call_with_retry(ak.gaming_tencent)

    def get_gaming_netease(self) -> Dict[str, Any]:
        """Get Netease gaming data"""
        return self._safe_call_with_retry(ak.gaming_netease)

    def get_gaming_steam(self) -> Dict[str, Any]:
        """Get Steam gaming data"""
        return self._safe_call_with_retry(ak.gaming_steam)

    # ==================== AUTOMOTIVE DATA ====================

    def get_other_car_cpca(self) -> Dict[str, Any]:
        """Get automotive industry data"""
        return self._safe_call_with_retry(ak.other_car_cpca)

    def get_car_sales_china(self) -> Dict[str, Any]:
        """Get China car sales data"""
        return self._safe_call_with_retry(ak.car_sales_china)

    def get_car_sales_monthly(self) -> Dict[str, Any]:
        """Get monthly car sales data"""
        return self._safe_call_with_retry(ak.car_sales_monthly)

    def get_car_sales_manufacturer(self) -> Dict[str, Any]:
        """Get car sales by manufacturer"""
        return self._safe_call_with_retry(ak.car_sales_manufacturer)

    def get_car_sales_model(self) -> Dict[str, Any]:
        """Get car sales by model"""
        return self._safe_call_with_retry(ak.car_sales_model)

    def get_car_production(self) -> Dict[str, Any]:
        """Get car production data"""
        return self._safe_call_with_retry(ak.car_production)

    def get_car_inventory(self) -> Dict[str, Any]:
        """Get car inventory data"""
        return self._safe_call_with_retry(ak.car_inventory)

    def get_car_price(self) -> Dict[str, Any]:
        """Get car price data"""
        return self._safe_call_with_retry(ak.car_price)

    def get_car_dealer(self) -> Dict[str, Any]:
        """Get car dealer data"""
        return self._safe_call_with_retry(ak.car_dealer)

    def get_car_insurance(self) -> Dict[str, Any]:
        """Get car insurance data"""
        return self._safe_call_with_retry(ak.car_insurance)

    # ==================== REAL ESTATE DATA ====================

    def get_reits_basic(self) -> Dict[str, Any]:
        """Get REITs basic data"""
        return self._safe_call_with_retry(ak.reits_basic)

    def get_reits_price(self) -> Dict[str, Any]:
        """Get REITs price data"""
        return self._safe_call_with_retry(ak.reits_price)

    def get_reits_yield(self) -> Dict[str, Any]:
        """Get REITs yield data"""
        return self._safe_call_with_retry(ak.reits_yield)

    def get_reits_portfolio(self) -> Dict[str, Any]:
        """Get REITs portfolio data"""
        return self._safe_call_with_retry(ak.reits_portfolio)

    def get_reits_distribution(self) -> Dict[str, Any]:
        """Get REITs distribution data"""
        return self._safe_call_with_retry(ak.reits_distribution)

    def get_real_estate_price(self) -> Dict[str, Any]:
        """Get real estate price data"""
        return self._safe_call_with_retry(ak.real_estate_price)

    def get_real_estate_rent(self) -> Dict[str, Any]:
        """Get real estate rent data"""
        return self._safe_call_with_retry(ak.real_estate_rent)

    def get_real_estate_transaction(self) -> Dict[str, Any]:
        """Get real estate transaction data"""
        return self._safe_call_with_retry(ak.real_estate_transaction)

    def get_real_estate_supply(self) -> Dict[str, Any]:
        """Get real estate supply data"""
        return self._safe_call_with_retry(ak.real_estate_supply)

    def get_real_estate_inventory(self) -> Dict[str, Any]:
        """Get real estate inventory data"""
        return self._safe_call_with_retry(ak.real_estate_inventory)

    def get_real_estate_mortgage(self) -> Dict[str, Any]:
        """Get real estate mortgage data"""
        return self._safe_call_with_retry(ak.real_estate_mortgage)

    # ==================== SOCIAL MEDIA AND SENTIMENT ====================

    def get_stock_hot_xq(self) -> Dict[str, Any]:
        """Get Xueqiu hot stocks"""
        return self._safe_call_with_retry(ak.stock_hot_xq)

    def get_stock_hot_eastmoney(self) -> Dict[str, Any]:
        """Get Eastmoney hot stocks"""
        return self._safe_call_with_retry(ak.stock_hot_eastmoney)

    def get_stock_hot_sina(self) -> Dict[str, Any]:
        """Get Sina hot stocks"""
        return self._safe_call_with_retry(ak.stock_hot_sina)

    def get_stock_comment_em(self) -> Dict[str, Any]:
        """Get Eastmoney stock comments"""
        return self._safe_call_with_retry(ak.stock_comment_em)

    def get_stock_comment_sina(self) -> Dict[str, Any]:
        """Get Sina stock comments"""
        return self._safe_call_with_retry(ak.stock_comment_sina)

    def get_stock_weibo_sentiment(self) -> Dict[str, Any]:
        """Get Weibo stock sentiment"""
        return self._safe_call_with_retry(ak.stock_weibo_sentiment)

    def get_stock_zhihu_sentiment(self) -> Dict[str, Any]:
        """Get Zhihu stock sentiment"""
        return self._safe_call_with_retry(ak.stock_zhihu_sentiment)

    def get_stock_xueqiu_sentiment(self) -> Dict[str, Any]:
        """Get Xueqiu stock sentiment"""
        return self._safe_call_with_retry(ak.stock_xueqiu_sentiment)

    def get_stock_guba_sentiment(self) -> Dict[str, Any]:
        """Get Guba stock sentiment"""
        return self._safe_call_with_retry(ak.stock_guba_sentiment)

    def get_news_stock_em(self) -> Dict[str, Any]:
        """Get Eastmoney stock news"""
        return self._safe_call_with_retry(ak.news_stock_em)

    def get_news_jin10(self) -> Dict[str, Any]:
        """Get Jin10 financial news"""
        return self._safe_call_with_retry(ak.news_jin10)

    def get_news_fx168(self) -> Dict[str, Any]:
        """Get FX168 financial news"""
        return self._safe_call_with_retry(ak.news_fx168)

    def get_news_sina(self) -> Dict[str, Any]:
        """Get Sina financial news"""
        return self._safe_call_with_retry(ak.news_sina)

    # ==================== SURVEYS AND POLLS ====================

    def get_stock_vote_baidu(self) -> Dict[str, Any]:
        """Get Baidu stock voting"""
        return self._safe_call_with_retry(ak.stock_vote_baidu)

    def get_survey_consumer(self) -> Dict[str, Any]:
        """Get consumer survey data"""
        return self._safe_call_with_retry(ak.survey_consumer)

    def get_survey_business(self) -> Dict[str, Any]:
        """Get business survey data"""
        return self._safe_call_with_retry(ak.survey_business)

    def get_survey_economic(self) -> Dict[str, Any]:
        """Get economic survey data"""
        return self._safe_call_with_retry(ak.survey_economic)

    def get_survey_investor(self) -> Dict[str, Any]:
        """Get investor survey data"""
        return self._safe_call_with_retry(ak.survey_investor)

    def get_poll_market(self) -> Dict[str, Any]:
        """Get market poll data"""
        return self._safe_call_with_retry(ak.poll_market)

    def get_poll_economy(self) -> Dict[str, Any]:
        """Get economy poll data"""
        return self._safe_call_with_retry(ak.poll_economy)

    # ==================== COMPANY RANKINGS ====================

    def get_fortune_500(self) -> Dict[str, Any]:
        """Get Fortune 500 companies"""
        return self._safe_call_with_retry(ak.fortune_500)

    def get_fortune_china_500(self) -> Dict[str, Any]:
        """Get Fortune China 500 companies"""
        return self._safe_call_with_retry(ak.fortune_china_500)

    def get_fortune_global_500(self) -> Dict[str, Any]:
        """Get Fortune Global 500 companies"""
        return self._safe_call_with_retry(ak.fortune_global_500)

    def get_forbes_global_2000(self) -> Dict[str, Any]:
        """Get Forbes Global 2000 companies"""
        return self._safe_call_with_retry(ak.forbes_global_2000)

    def get_forbes_china_400(self) -> Dict[str, Any]:
        """Get Forbes China 400 rich list"""
        return self._safe_call_with_retry(ak.forbes_china_400)

    def get_billionaires_list(self) -> Dict[str, Any]:
        """Get billionaires list"""
        return self._safe_call_with_retry(ak.billionaires_list)

    def get_hurun_rich_list(self) -> Dict[str, Any]:
        """Get Hurun rich list"""
        return self._safe_call_with_retry(ak.hurun_rich_list)

    # ==================== WEATHER AND CLIMATE ====================

    def get_weather_today(self, city: str) -> Dict[str, Any]:
        """Get today's weather data"""
        return self._safe_call_with_retry(ak.weather_today, city=city)

    def get_weather_forecast(self, city: str) -> Dict[str, Any]:
        """Get weather forecast data"""
        return self._safe_call_with_retry(ak.weather_forecast, city=city)

    def get_weather_historical(self, city: str, start_date: str = None, end_date: str = None) -> Dict[str, Any]:
        """Get historical weather data"""
        start = start_date or self.default_start_date
        end = end_date or self.default_end_date
        return self._safe_call_with_retry(ak.weather_historical, city=city, start_date=start, end_date=end)

    def get_weather_cities(self) -> Dict[str, Any]:
        """Get weather city list"""
        return self._safe_call_with_retry(ak.weather_cities)

    def get_weather_alerts(self) -> Dict[str, Any]:
        """Get weather alerts"""
        return self._safe_call_with_retry(ak.weather_alerts)

    def get_climate_data(self) -> Dict[str, Any]:
        """Get climate data"""
        return self._safe_call_with_retry(ak.climate_data)

    def get_temperature_anomaly(self) -> Dict[str, Any]:
        """Get temperature anomaly data"""
        return self._safe_call_with_retry(ak.temperature_anomaly)

    # ==================== DEMOGRAPHIC AND SOCIAL DATA ====================

    def get_population_china(self) -> Dict[str, Any]:
        """Get China population data"""
        return self._safe_call_with_retry(ak.population_china)

    def get_population_city(self) -> Dict[str, Any]:
        """Get city population data"""
        return self._safe_call_with_retry(ak.population_city)

    def get_population_province(self) -> Dict[str, Any]:
        """Get province population data"""
        return self._safe_call_with_retry(ak.population_province)

    def get_demographics_age(self) -> Dict[str, Any]:
        """Get age demographics"""
        return self._safe_call_with_retry(ak.demographics_age)

    def get_demographics_education(self) -> Dict[str, Any]:
        """Get education demographics"""
        return self._safe_call_with_retry(ak.demographics_education)

    def get_demographics_income(self) -> Dict[str, Any]:
        """Get income demographics"""
        return self._safe_call_with_retry(ak.demographics_income)

    def get_demographics_urbanization(self) -> Dict[str, Any]:
        """Get urbanization data"""
        return self._safe_call_with_retry(ak.demographics_urbanization)

    def get_social_security(self) -> Dict[str, Any]:
        """Get social security data"""
        return self._safe_call_with_retry(ak.social_security)

    def get_pension_data(self) -> Dict[str, Any]:
        """Get pension data"""
        return self._safe_call_with_retry(ak.pension_data)

    def get_healthcare_data(self) -> Dict[str, Any]:
        """Get healthcare data"""
        return self._safe_call_with_retry(ak.healthcare_data)

    # ==================== UTILITIES ====================

    def get_all_available_endpoints(self) -> Dict[str, Any]:
        """Get list of all available endpoints in this wrapper"""
        methods = [method for method in dir(self) if method.startswith('get_') and callable(getattr(self, method))]
        return {
            "available_endpoints": methods,
            "total_count": len(methods),
            "categories": {
                "Environmental Data": [m for m in methods if any(x in m for x in ["air", "energy", "carbon", "oil", "coal", "solar", "wind", "hydro"])],
                "Consumer Data": [m for m in methods if any(x in m for x in ["cost_living", "movie", "music", "gaming"])],
                "Automotive Data": [m for m in methods if "car" in m],
                "Real Estate": [m for m in methods if any(x in m for x in ["reits", "real_estate"])],
                "Social Media & Sentiment": [m for m in methods if any(x in m for x in ["hot", "comment", "sentiment", "news"])],
                "Surveys & Polls": [m for m in methods if any(x in m for x in ["survey", "poll", "vote"])],
                "Company Rankings": [m for m in methods if any(x in m for x in ["fortune", "forbes", "billionaires", "hurun"])],
                "Weather & Climate": [m for m in methods if any(x in m for x in ["weather", "climate", "temperature"])],
                "Demographics & Social": [m for m in methods if any(x in m for x in ["population", "demographics", "social", "pension", "healthcare"])]
            },
            "timestamp": int(datetime.now().timestamp())
        }

    

# ==================== COMMAND LINE INTERFACE ====================

def main():
    """Command line interface for the Alternative Data wrapper"""
    wrapper = AlternativeDataWrapper()

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python akshare_alternative.py <endpoint> [args...]",
            "available_endpoints": wrapper.get_all_available_endpoints()["available_endpoints"]
        }, indent=2))
        return

    endpoint = sys.argv[1]
    args = sys.argv[2:] if len(sys.argv) > 2 else []

    # Map endpoint names to method calls
    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_available_endpoints,
        # Environmental Data
        "air_quality": wrapper.get_air_quality_hebei,
        "air_nationwide": wrapper.get_air_zhenqi,
        "air_daily": wrapper.get_air_china_daily,
        "air_ranking": wrapper.get_air_city_ranking,
        "air_pollution": wrapper.get_air_pollution_index,
        "carbon": wrapper.get_energy_carbon,
        "carbon_price": wrapper.get_energy_carbon_price,
        "carbon_quota": wrapper.get_energy_carbon_quota,
        "oil": wrapper.get_energy_oil_em,
        "oil_ice": wrapper.get_energy_oil_ice,
        "natural_gas": wrapper.get_energy_natural_gas,
        "coal": wrapper.get_energy_coal,
        "electricity": wrapper.get_energy_electricity,
        "solar": wrapper.get_energy_solar,
        "wind": wrapper.get_energy_wind,
        "hydro": wrapper.get_energy_hydro,
        # Consumer Data
        "cost_living": wrapper.get_cost_living,
        "cost_city": wrapper.get_cost_living_city,
        "cost_country": wrapper.get_cost_living_country,
        "movie_boxoffice": wrapper.get_movie_yien,
        "movie_maoyan": wrapper.get_movie_maoyan,
        "movie_douban": wrapper.get_movie_douban,
        "movie_ranking": wrapper.get_movie_ranking,
        "music_qq": wrapper.get_music_qq,
        "music_netease": wrapper.get_music_netease,
        "gaming_tencent": wrapper.get_gaming_tencent,
        "gaming_steam": wrapper.get_gaming_steam,
        # Automotive Data
        "automotive": wrapper.get_other_car_cpca,
        "car_sales": wrapper.get_car_sales_china,
        "car_sales_monthly": wrapper.get_car_sales_monthly,
        "car_sales_manufacturer": wrapper.get_car_sales_manufacturer,
        "car_sales_model": wrapper.get_car_sales_model,
        "car_production": wrapper.get_car_production,
        "car_inventory": wrapper.get_car_inventory,
        "car_price": wrapper.get_car_price,
        "car_dealer": wrapper.get_car_dealer,
        # Real Estate
        "reits_basic": wrapper.get_reits_basic,
        "reits_price": wrapper.get_reits_price,
        "reits_yield": wrapper.get_reits_yield,
        "reits_portfolio": wrapper.get_reits_portfolio,
        "reits_distribution": wrapper.get_reits_distribution,
        "real_estate_price": wrapper.get_real_estate_price,
        "real_estate_rent": wrapper.get_real_estate_rent,
        "real_estate_transaction": wrapper.get_real_estate_transaction,
        # Social Media & Sentiment
        "hot_stocks_xq": wrapper.get_stock_hot_xq,
        "hot_stocks_em": wrapper.get_stock_hot_eastmoney,
        "hot_stocks_sina": wrapper.get_stock_hot_sina,
        "stock_comments": wrapper.get_stock_comment_em,
        "stock_sentiment_weibo": wrapper.get_stock_weibo_sentiment,
        "stock_sentiment_zhihu": wrapper.get_stock_zhihu_sentiment,
        "stock_sentiment_xueqiu": wrapper.get_stock_xueqiu_sentiment,
        "news_stock": wrapper.get_news_stock_em,
        "news_jin10": wrapper.get_news_jin10,
        "news_fx168": wrapper.get_news_fx168,
        # Surveys & Polls
        "stock_vote": wrapper.get_stock_vote_baidu,
        "survey_consumer": wrapper.get_survey_consumer,
        "survey_business": wrapper.get_survey_business,
        "survey_investor": wrapper.get_survey_investor,
        "poll_market": wrapper.get_poll_market,
        # Company Rankings
        "fortune_500": wrapper.get_fortune_500,
        "fortune_china_500": wrapper.get_fortune_china_500,
        "fortune_global_500": wrapper.get_fortune_global_500,
        "forbes_global_2000": wrapper.get_forbes_global_2000,
        "forbes_china_400": wrapper.get_forbes_china_400,
        "billionaires": wrapper.get_billionaires_list,
        "hurun_rich": wrapper.get_hurun_rich_list,
        # Weather & Climate
        "weather_today": wrapper.get_weather_today,
        "weather_forecast": wrapper.get_weather_forecast,
        "weather_historical": wrapper.get_weather_historical,
        "weather_cities": wrapper.get_weather_cities,
        "weather_alerts": wrapper.get_weather_alerts,
        "climate_data": wrapper.get_climate_data,
        "temperature_anomaly": wrapper.get_temperature_anomaly,
        # Demographics & Social
        "population": wrapper.get_population_china,
        "population_city": wrapper.get_population_city,
        "population_province": wrapper.get_population_province,
        "demographics_age": wrapper.get_demographics_age,
        "demographics_education": wrapper.get_demographics_education,
        "demographics_income": wrapper.get_demographics_income,
        "urbanization": wrapper.get_demographics_urbanization,
        "social_security": wrapper.get_social_security,
        "pension": wrapper.get_pension_data,
        "healthcare": wrapper.get_healthcare_data
    }

    method = endpoint_map.get(endpoint)
    if method:
        if args:
            try:
                result = method(*args)
            except Exception as e:
                result = {"error": str(e), "endpoint": endpoint}
        else:
            result = method()
        print(json.dumps(result, indent=2, ensure_ascii=True))
    else:
        print(json.dumps({
            "error": f"Unknown endpoint: {endpoint}",
            "available_endpoints": list(endpoint_map.keys())
        }, indent=2))


if __name__ == "__main__":
    main()