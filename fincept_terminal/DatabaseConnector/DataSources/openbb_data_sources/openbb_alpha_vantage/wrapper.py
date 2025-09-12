"""Alpha Vantage Wrapper for Finance Terminal"""

import asyncio
import pandas as pd
from datetime import datetime, date
from typing import Dict, List, Optional, Union, Any
from dataclasses import dataclass
import warnings

from openbb_alpha_vantage.models.equity_historical import (
    AVEquityHistoricalFetcher,
    AVEquityHistoricalQueryParams
)
from openbb_alpha_vantage.models.historical_eps import (
    AVHistoricalEpsFetcher,
    AlphaVantageHistoricalEpsQueryParams
)


@dataclass
class WrapperResponse:
    success: bool
    data: Optional[Union[pd.DataFrame, Dict, List]] = None
    error: Optional[str] = None
    message: Optional[str] = None


class AlphaVantageWrapper:

    def __init__(self, api_key: str):
        self.api_key = api_key
        self.credentials = {"alpha_vantage_api_key": api_key}
        warnings.filterwarnings("ignore")

    def _run_async(self, coro):
        try:
            loop = asyncio.get_event_loop()
        except RuntimeError:
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
        return loop.run_until_complete(coro)

    def _validate_dates(self, start_date, end_date):
        if isinstance(start_date, str):
            start_date = datetime.strptime(start_date, "%Y-%m-%d").date()
        if isinstance(end_date, str):
            end_date = datetime.strptime(end_date, "%Y-%m-%d").date()
        return start_date, end_date

    def get_equity_historical(self, **params) -> WrapperResponse:
        """Dynamic equity historical data fetcher"""
        try:
            defaults = {
                "interval": "1d",
                "adjustment": "splits_only",
                "extended_hours": False
            }

            query_params = {**defaults, **params}

            if "start_date" in query_params and "end_date" in query_params:
                query_params["start_date"], query_params["end_date"] = self._validate_dates(
                    query_params["start_date"], query_params["end_date"]
                )

            query = AVEquityHistoricalFetcher.transform_query(query_params)
            raw_data = self._run_async(
                AVEquityHistoricalFetcher.aextract_data(query, self.credentials)
            )
            transformed_data = AVEquityHistoricalFetcher.transform_data(query, raw_data)

            if not transformed_data:
                return WrapperResponse(success=False, error="No data found")

            df = pd.DataFrame([item.model_dump() for item in transformed_data])
            df['date'] = pd.to_datetime(df['date'])

            if 'symbol' not in df.columns:
                df.set_index('date', inplace=True)

            return WrapperResponse(
                success=True,
                data=df,
                message=f"Retrieved {len(df)} records"
            )

        except Exception as e:
            return WrapperResponse(success=False, error=str(e))

    def get_historical_eps(self, **params) -> WrapperResponse:
        """Dynamic EPS data fetcher"""
        try:
            defaults = {
                "period": "quarter",
                "limit": None
            }

            query_params = {**defaults, **params}

            query = AVHistoricalEpsFetcher.transform_query(query_params)
            raw_data = self._run_async(
                AVHistoricalEpsFetcher.aextract_data(query, self.credentials)
            )
            transformed_data = AVHistoricalEpsFetcher.transform_data(query, raw_data)

            if not transformed_data:
                return WrapperResponse(success=False, error="No EPS data found")

            df = pd.DataFrame([item.model_dump() for item in transformed_data])
            df['date'] = pd.to_datetime(df['date'])
            df.sort_values('date', ascending=False, inplace=True)

            return WrapperResponse(
                success=True,
                data=df,
                message=f"Retrieved {len(df)} EPS records"
            )

        except Exception as e:
            return WrapperResponse(success=False, error=str(e))

    def get_etf_historical(self, **params) -> WrapperResponse:
        """Dynamic ETF data fetcher"""
        params.setdefault("extended_hours", False)
        return self.get_equity_historical(**params)

    def execute_query(self, data_type: str, **params) -> WrapperResponse:
        """Generic query executor"""
        method_map = {
            "equity": self.get_equity_historical,
            "eps": self.get_historical_eps,
            "etf": self.get_etf_historical
        }

        if data_type not in method_map:
            return WrapperResponse(success=False, error=f"Unknown data type: {data_type}")

        return method_map[data_type](**params)

    def test_connection(self) -> WrapperResponse:
        try:
            result = self.get_equity_historical(
                symbol="AAPL",
                start_date=datetime.now().date().replace(day=1),
                end_date=datetime.now().date(),
                interval="1d"
            )

            if result.success:
                return WrapperResponse(success=True, message="Connection successful")
            else:
                return WrapperResponse(success=False, error="Connection failed")

        except Exception as e:
            return WrapperResponse(success=False, error=str(e))

    @staticmethod
    def get_schema() -> Dict:
        return {
            "equity_historical": {
                "required": ["symbol"],
                "optional": {
                    "start_date": "YYYY-MM-DD or date object",
                    "end_date": "YYYY-MM-DD or date object",
                    "interval": ["1m", "5m", "15m", "30m", "60m", "1d", "1W", "1M"],
                    "adjustment": ["splits_only", "splits_and_dividends", "unadjusted"],
                    "extended_hours": "bool"
                }
            },
            "historical_eps": {
                "required": ["symbol"],
                "optional": {
                    "period": ["annual", "quarter"],
                    "limit": "int"
                }
            },
            "etf_historical": {
                "required": ["symbol"],
                "optional": {
                    "start_date": "YYYY-MM-DD or date object",
                    "end_date": "YYYY-MM-DD or date object",
                    "interval": ["1m", "5m", "15m", "30m", "60m", "1d", "1W", "1M"],
                    "adjustment": ["splits_only", "splits_and_dividends", "unadjusted"]
                }
            }
        }


class WrapperFactory:
    """Factory for creating provider wrappers"""

    @staticmethod
    def create_alpha_vantage(api_key: str) -> AlphaVantageWrapper:
        return AlphaVantageWrapper(api_key)

    @staticmethod
    def get_available_providers() -> List[str]:
        return ["alpha_vantage"]


# Dynamic function builder
def build_dynamic_call(wrapper: AlphaVantageWrapper, data_type: str):
    """Creates a dynamic callable for specific data type"""
    return lambda **kwargs: wrapper.execute_query(data_type, **kwargs)


# Runtime parameter validator
class ParameterValidator:

    @staticmethod
    def validate_params(data_type: str, params: Dict[str, Any]) -> Dict[str, Any]:
        schema = AlphaVantageWrapper.get_schema()

        if data_type not in schema:
            raise ValueError(f"Unknown data type: {data_type}")

        type_schema = schema[data_type]

        # Check required parameters
        for required in type_schema["required"]:
            if required not in params:
                raise ValueError(f"Missing required parameter: {required}")

        # Validate optional parameters
        for param, value in params.items():
            if param in type_schema["optional"]:
                expected = type_schema["optional"][param]
                if isinstance(expected, list) and value not in expected:
                    raise ValueError(f"Invalid value for {param}: {value}. Expected one of {expected}")

        return params