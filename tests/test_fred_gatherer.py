"""Tests for FRED data gatherer - FinceptTerminal#224 MVP"""

import pytest
import pandas as pd
import numpy as np
from unittest.mock import patch, MagicMock
from fred_gatherer import FREDGatherer, FREDConfig, SeriesMetadata


class TestFREDConfig:
    def test_default_config(self):
        config = FREDConfig(api_key="test_key")
        assert config.api_key == "test_key"
        assert config.base_url == "https://api.stlouisfed.org/fred"
        assert config.cache_ttl == 3600
        assert config.timeout == 30

    def test_env_api_key(self):
        import os
        os.environ["FRED_API_KEY"] = "env_key"
        config = FREDConfig()
        assert config.api_key == "env_key"
        del os.environ["FRED_API_KEY"]


class TestSeriesMetadata:
    def test_metadata_creation(self):
        meta = SeriesMetadata(
            id="GDP",
            title="Gross Domestic Product",
            observation_start="1947-01-01",
            observation_end="2024-01-01",
            frequency="Annual",
            frequency_short="A",
            units="Billions of Dollars",
            units_short="Billions of Dollars",
            last_updated="2024-01-25 08:24:07-06",
            popularity=90,
        )
        assert meta.id == "GDP"
        assert meta.popularity == 90


class TestFREDGatherer:
    def _make_gatherer(self):
        return FREDGatherer(api_key="test_key")

    def _mock_api_response(self, observations=None, series=None):
        if observations is not None:
            return {"observations": observations}
        if series is not None:
            return {"series": series}
        return {"observations": []}

    @patch.object(FREDGatherer, '_api_request')
    def test_get_series(self, mock_request):
        gatherer = self._make_gatherer()
        mock_request.return_value = self._mock_api_response(observations=[
            {"date": "2024-01-01", "value": "27000.0"},
            {"date": "2024-04-01", "value": "27500.0"},
            {"date": "2024-07-01", "value": "."},  # Missing value
        ])

        result = gatherer.get_series("GDP")
        assert isinstance(result, pd.Series)
        assert len(result) == 2  # Missing value excluded
        assert result.name == "GDP"
        assert result.iloc[0] == 27000.0

    @patch.object(FREDGatherer, '_api_request')
    def test_get_series_empty(self, mock_request):
        gatherer = self._make_gatherer()
        mock_request.return_value = self._mock_api_response(observations=[])

        result = gatherer.get_series("NONEXISTENT")
        assert isinstance(result, pd.Series)
        assert len(result) == 0

    @patch.object(FREDGatherer, '_api_request')
    def test_get_series_metadata(self, mock_request):
        gatherer = self._make_gatherer()
        mock_request.return_value = self._mock_api_response(series={
            "id": "GDP",
            "title": "Gross Domestic Product",
            "observation_start": "1947-01-01",
            "observation_end": "2024-01-01",
            "frequency": "Annual",
            "frequency_short": "A",
            "units": "Billions of Dollars",
            "units_short": "Billions of Dollars",
            "last_updated": "2024-01-25",
            "popularity": 90,
            "notes": "",
        })

        meta = gatherer.get_series_metadata("GDP")
        assert meta.id == "GDP"
        assert meta.title == "Gross Domestic Product"
        assert isinstance(meta, SeriesMetadata)

    @patch.object(FREDGatherer, '_api_request')
    def test_get_indicators(self, mock_request):
        gatherer = self._make_gatherer()

        def side_effect(endpoint, params):
            if "CPIAUCSL" in params.get("series_id", ""):
                return self._mock_api_response(observations=[
                    {"date": "2024-01-01", "value": "310.0"},
                ])
            elif "FEDFUNDS" in params.get("series_id", ""):
                return self._mock_api_response(observations=[
                    {"date": "2024-01-01", "value": "5.33"},
                ])
            return self._mock_api_response(observations=[])

        mock_request.side_effect = side_effect

        result = gatherer.get_indicators(["CPIAUCSL", "FEDFUNDS"])
        assert isinstance(result, pd.DataFrame)
        assert "CPIAUCSL" in result.columns
        assert "FEDFUNDS" in result.columns

    @patch.object(FREDGatherer, '_api_request')
    def test_search(self, mock_request):
        gatherer = self._make_gatherer()
        mock_request.return_value = {
            "seriess": [
                {"id": "GDP", "title": "Gross Domestic Product", "popularity": 90},
                {"id": "GDPC1", "title": "Real GDP", "popularity": 85},
            ]
        }

        results = gatherer.search("GDP")
        assert len(results) == 2
        assert results[0]["id"] == "GDP"

    def test_valuation_indicators_list(self):
        gatherer = self._make_gatherer()
        assert "gdp" in gatherer.VALUATION_INDICATORS
        assert "cpi" in gatherer.VALUATION_INDICATORS
        assert "fed_funds_rate" in gatherer.VALUATION_INDICATORS
        assert len(gatherer.VALUATION_INDICATORS) >= 20

    def test_calculate_spread(self):
        gatherer = self._make_gatherer()
        dates = pd.date_range("2024-01-01", periods=5, freq="ME")
        s1 = pd.Series([5.0, 4.8, 4.5, 4.3, 4.0], index=dates, name="10y")
        s2 = pd.Series([4.5, 4.3, 4.0, 3.8, 3.5], index=dates, name="2y")

        spread = gatherer.calculate_spread(s1, s2)
        assert len(spread) == 5
        assert abs(spread.iloc[0] - 0.5) < 0.001

    def test_calculate_yoy_growth(self):
        gatherer = self._make_gatherer()
        dates = pd.date_range("2020-01-01", periods=13, freq="ME")
        values = [100] * 12 + [105]  # 5% growth after 12 months
        series = pd.Series(values, index=dates)

        growth = gatherer.calculate_yoy_growth(series, periods=12)
        assert abs(growth.iloc[-1] - 5.0) < 0.001

    def test_calculate_real_value(self):
        gatherer = self._make_gatherer()
        dates = pd.date_range("2020-01-01", periods=3, freq="YE")
        nominal = pd.Series([100, 105, 110], index=dates)
        cpi = pd.Series([100, 103, 106], index=dates)

        real = gatherer.calculate_real_value(nominal, cpi)
        assert len(real) == 3
        assert abs(real.iloc[0] - 100.0) < 0.001


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
