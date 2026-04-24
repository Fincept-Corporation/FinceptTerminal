import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock, patch


SCRIPT_PATH = Path(__file__).resolve().parents[1] / "fred_data.py"
MODULE_SPEC = importlib.util.spec_from_file_location("fred_data", SCRIPT_PATH)
fred_data = importlib.util.module_from_spec(MODULE_SPEC)
assert MODULE_SPEC is not None and MODULE_SPEC.loader is not None


class _DummySession:
    def mount(self, *_args, **_kwargs) -> None:
        return None

    def get(self, *_args, **_kwargs):
        raise NotImplementedError


dummy_requests = SimpleNamespace(
    Session=lambda: _DummySession(),
    adapters=SimpleNamespace(HTTPAdapter=lambda **_kwargs: object()),
    exceptions=SimpleNamespace(
        Timeout=type("DummyTimeout", (Exception,), {}),
        RequestException=type("DummyRequestException", (Exception,), {}),
    ),
)

with patch.dict(sys.modules, {"requests": dummy_requests}):
    MODULE_SPEC.loader.exec_module(fred_data)


class TestFredData(unittest.TestCase):
    def test_missing_api_key_returns_structured_error(self) -> None:
        with patch.object(fred_data, "FRED_API_KEY", ""):
            result = fred_data.make_fred_request("series", {"series_id": "UNRATE"})
        self.assertFalse(result["success"])
        self.assertEqual(result["error_code"], "FRED_AUTH_MISSING")

    def test_rate_limit_returns_structured_error(self) -> None:
        fake_response = Mock()
        fake_response.status_code = 429
        with (
            patch.object(fred_data, "FRED_API_KEY", "test-key"),
            patch.object(fred_data.session, "get", return_value=fake_response),
        ):
            result = fred_data.make_fred_request("series", {"series_id": "UNRATE"})
        self.assertFalse(result["success"])
        self.assertEqual(result["error_code"], "FRED_RATE_LIMIT")

    def test_get_series_aligns_quarterly_to_monthly(self) -> None:
        obs_payload = {
            "observation_start": "2020-01-01",
            "observation_end": "2020-06-30",
            "observations": [
                {"date": "2020-01-01", "value": "100"},
                {"date": "2020-04-01", "value": "110"},
            ],
        }
        meta_payload = {
            "seriess": [
                {
                    "title": "GDP",
                    "units": "Billions",
                    "frequency": "Quarterly",
                    "frequency_short": "Q",
                    "seasonal_adjustment": "SAAR",
                    "last_updated": "2026-01-01",
                }
            ]
        }
        with patch.object(fred_data, "make_fred_request", side_effect=[obs_payload, meta_payload]):
            result = fred_data.get_series("GDP", "2020-01-01", "2020-06-30", "m", None)

        self.assertTrue(result["success"])
        self.assertEqual(result["observation_count"], 4)
        self.assertEqual(
            [o["date"] for o in result["observations"]],
            ["2020-01-31", "2020-02-29", "2020-03-31", "2020-04-30"],
        )
        self.assertEqual([o["value"] for o in result["observations"]], [100.0, 100.0, 100.0, 110.0])

    def test_latest_command_returns_latest_point(self) -> None:
        series_payload = {
            "success": True,
            "title": "Unemployment Rate",
            "units": "Percent",
            "observation_count": 2,
            "latest": {"date": "2026-02-01", "value": 4.1},
        }
        with patch.object(fred_data, "get_series", return_value=series_payload):
            result = fred_data.get_latest_series_value("UNRATE", None, None)
        self.assertTrue(result["success"])
        self.assertEqual(result["latest"]["value"], 4.1)
        self.assertEqual(result["series_id"], "UNRATE")

    def test_make_fred_request_uses_cache(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            cache_dir = Path(temp_dir)
            payload = {"observations": [{"date": "2024-01-01", "value": "5.0"}]}

            fake_response = Mock()
            fake_response.status_code = 200
            fake_response.raise_for_status = Mock()
            fake_response.json.return_value = payload

            with (
                patch.object(fred_data, "FRED_API_KEY", "test-key"),
                patch.object(fred_data, "CACHE_DIR", cache_dir),
                patch.object(fred_data.session, "get", return_value=fake_response) as mock_get,
            ):
                first = fred_data.make_fred_request("series/observations", {"series_id": "UNRATE"})
                second = fred_data.make_fred_request("series/observations", {"series_id": "UNRATE"})

            self.assertEqual(first, payload)
            self.assertEqual(second, payload)
            self.assertEqual(mock_get.call_count, 1)

    def test_series_not_found_maps_to_specific_error_code(self) -> None:
        fake_response = Mock()
        fake_response.status_code = 200
        fake_response.raise_for_status = Mock()
        fake_response.json.return_value = {
            "error_code": 400,
            "error_message": "The series does not exist.",
        }
        with (
            patch.object(fred_data, "FRED_API_KEY", "test-key"),
            patch.object(fred_data.session, "get", return_value=fake_response),
        ):
            result = fred_data.make_fred_request("series/observations", {"series_id": "BAD_SERIES"})
        self.assertFalse(result["success"])
        self.assertEqual(result["error_code"], "FRED_SERIES_NOT_FOUND")

    def test_bad_request_maps_to_fred_bad_request(self) -> None:
        fake_response = Mock()
        fake_response.status_code = 200
        fake_response.raise_for_status = Mock()
        fake_response.json.return_value = {
            "error_code": 400,
            "error_message": "Bad request for testing.",
        }
        with (
            patch.object(fred_data, "FRED_API_KEY", "test-key"),
            patch.object(fred_data.session, "get", return_value=fake_response),
        ):
            result = fred_data.make_fred_request("series/observations", {"series_id": "UNRATE"})
        self.assertFalse(result["success"])
        self.assertEqual(result["error_code"], "FRED_BAD_REQUEST")

    def test_json_decode_failure_maps_to_bad_json(self) -> None:
        fake_response = Mock()
        fake_response.status_code = 200
        fake_response.raise_for_status = Mock()
        fake_response.json.side_effect = ValueError("invalid json")
        with (
            patch.object(fred_data, "FRED_API_KEY", "test-key"),
            patch.object(fred_data.session, "get", return_value=fake_response),
        ):
            result = fred_data.make_fred_request("series/observations", {"series_id": "UNRATE"})
        self.assertFalse(result["success"])
        self.assertEqual(result["error_code"], "FRED_BAD_JSON")

    def test_latest_no_data_maps_to_no_data_error(self) -> None:
        series_payload = {"success": True, "latest": None}
        with patch.object(fred_data, "get_series", return_value=series_payload):
            result = fred_data.get_latest_series_value("UNRATE", None, None)
        self.assertFalse(result["success"])
        self.assertEqual(result["error_code"], "FRED_NO_DATA")

    def test_get_multiple_series_preserves_order(self) -> None:
        def _fake_get_series(series_id, *_args, **_kwargs):
            return {"success": True, "series_id": series_id}

        with patch.object(fred_data, "get_series", side_effect=_fake_get_series):
            result = fred_data.get_multiple_series(["GDP", "UNRATE", "CPIAUCSL"])
        self.assertEqual([entry["series_id"] for entry in result], ["GDP", "UNRATE", "CPIAUCSL"])

    def test_get_series_preserves_float_precision(self) -> None:
        obs_payload = {
            "observation_start": "2024-01-01",
            "observation_end": "2024-01-01",
            "observations": [
                {"date": "2024-01-01", "value": "0.1234567891234567"},
            ],
        }
        meta_payload = {
            "seriess": [
                {
                    "title": "Precision Series",
                    "units": "Index",
                    "frequency": "Monthly",
                    "frequency_short": "M",
                    "seasonal_adjustment": "NSA",
                    "last_updated": "2026-01-01",
                }
            ]
        }
        with patch.object(fred_data, "make_fred_request", side_effect=[obs_payload, meta_payload]):
            result = fred_data.get_series("PRECISION")
        self.assertTrue(result["success"])
        self.assertAlmostEqual(result["observations"][0]["value"], 0.1234567891234567, places=15)

    def test_get_series_skips_non_numeric_and_dot_values(self) -> None:
        obs_payload = {
            "observation_start": "2024-01-01",
            "observation_end": "2024-03-01",
            "observations": [
                {"date": "2024-01-01", "value": "."},
                {"date": "2024-02-01", "value": "not-a-number"},
                {"date": "2024-03-01", "value": "3.14"},
            ],
        }
        meta_payload = {
            "seriess": [
                {
                    "title": "Filter Series",
                    "units": "Value",
                    "frequency": "Monthly",
                    "frequency_short": "M",
                    "seasonal_adjustment": "NSA",
                    "last_updated": "2026-01-01",
                }
            ]
        }
        with patch.object(fred_data, "make_fred_request", side_effect=[obs_payload, meta_payload]):
            result = fred_data.get_series("FILTER")
        self.assertEqual(result["observation_count"], 1)
        self.assertEqual(result["observations"][0]["value"], 3.14)


if __name__ == "__main__":
    unittest.main()
