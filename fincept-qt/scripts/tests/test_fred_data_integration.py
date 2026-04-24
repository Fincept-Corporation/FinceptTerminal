import contextlib
import importlib.util
import io
import json
import sys
import unittest
from datetime import datetime as real_datetime
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch


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


def _run_main_and_capture(args):
    stream = io.StringIO()
    with contextlib.redirect_stdout(stream):
        try:
            fred_data.main(args)
            return stream.getvalue(), 0
        except SystemExit as exc:
            code = exc.code if isinstance(exc.code, int) else 1
            return stream.getvalue(), code


class TestFredDataIntegration(unittest.TestCase):
    def test_latest_command_outputs_json(self) -> None:
        expected = {
            "success": True,
            "series_id": "UNRATE",
            "latest": {"date": "2026-01-01", "value": 4.2},
            "units": "Percent",
            "title": "Unemployment Rate",
            "observation_count": 1,
        }
        with patch.object(fred_data, "get_latest_series_value", return_value=expected):
            out, code = _run_main_and_capture(["latest", "UNRATE"])
        self.assertEqual(code, 0)
        payload = json.loads(out.strip())
        self.assertEqual(payload["series_id"], "UNRATE")
        self.assertEqual(payload["latest"]["value"], 4.2)

    def test_historical_command_routes_arguments(self) -> None:
        expected = {"success": True, "series_id": "GDP", "observations": []}
        with patch.object(fred_data, "get_series", return_value=expected) as mock_get_series:
            out, code = _run_main_and_capture(["historical", "GDP", "2020-01-01", "2021-01-01", "m", "pch"])
        self.assertEqual(code, 0)
        mock_get_series.assert_called_once_with("GDP", "2020-01-01", "2021-01-01", "m", "pch")
        payload = json.loads(out.strip())
        self.assertTrue(payload["success"])
        self.assertEqual(payload["series_id"], "GDP")

    def test_usage_error_for_missing_required_args(self) -> None:
        out, code = _run_main_and_capture(["historical", "GDP"])
        self.assertEqual(code, 1)
        payload = json.loads(out.strip())
        self.assertFalse(payload["success"])
        self.assertEqual(payload["error_code"], "USAGE_ERROR")

    def test_unknown_command_returns_usage_error(self) -> None:
        out, code = _run_main_and_capture(["not_a_real_command"])
        self.assertEqual(code, 1)
        payload = json.loads(out.strip())
        self.assertFalse(payload["success"])
        self.assertEqual(payload["error_code"], "USAGE_ERROR")

    def test_series_command_routes_arguments(self) -> None:
        expected = {"success": True, "series_id": "UNRATE", "observations": []}
        with patch.object(fred_data, "get_series", return_value=expected) as mock_get_series:
            out, code = _run_main_and_capture(["series", "UNRATE", "2021-01-01", "2022-01-01", "m", "pch"])
        self.assertEqual(code, 0)
        mock_get_series.assert_called_once_with("UNRATE", "2021-01-01", "2022-01-01", "m", "pch")
        self.assertEqual(json.loads(out.strip())["series_id"], "UNRATE")

    def test_multiple_command_parses_dates_and_series(self) -> None:
        expected = [{"success": True, "series_id": "GDP"}, {"success": True, "series_id": "UNRATE"}]
        with patch.object(fred_data, "get_multiple_series", return_value=expected) as mock_multiple:
            out, code = _run_main_and_capture(["multiple", "GDP", "UNRATE", "2020-01-01", "2021-01-01"])
        self.assertEqual(code, 0)
        mock_multiple.assert_called_once_with(["GDP", "UNRATE"], "2020-01-01", "2021-01-01")
        self.assertEqual(len(json.loads(out.strip())), 2)

    def test_search_command_routes_limit(self) -> None:
        expected = {"success": True, "count": 1, "series": [{"id": "UNRATE"}]}
        with patch.object(fred_data, "search_series", return_value=expected) as mock_search:
            out, code = _run_main_and_capture(["search", "unemployment", "7"])
        self.assertEqual(code, 0)
        mock_search.assert_called_once_with("unemployment", 7)
        self.assertEqual(json.loads(out.strip())["count"], 1)

    def test_categories_command_routes_optional_id(self) -> None:
        expected = {"success": True, "category_id": 0, "categories": []}
        with patch.object(fred_data, "get_categories", return_value=expected) as mock_categories:
            out, code = _run_main_and_capture(["categories"])
        self.assertEqual(code, 0)
        mock_categories.assert_called_once_with(0)
        self.assertEqual(json.loads(out.strip())["category_id"], 0)

    def test_category_series_command_routes_limit(self) -> None:
        expected = {"success": True, "category_id": 125, "series": []}
        with patch.object(fred_data, "get_category_series", return_value=expected) as mock_category_series:
            out, code = _run_main_and_capture(["category_series", "125", "12"])
        self.assertEqual(code, 0)
        mock_category_series.assert_called_once_with(125, 12)
        self.assertEqual(json.loads(out.strip())["category_id"], 125)

    def test_releases_command_routes_limit(self) -> None:
        expected = {"success": True, "release_id": 53, "dates": []}
        with patch.object(fred_data, "get_release_dates", return_value=expected) as mock_releases:
            out, code = _run_main_and_capture(["releases", "53", "4"])
        self.assertEqual(code, 0)
        mock_releases.assert_called_once_with(53, 4)
        self.assertEqual(json.loads(out.strip())["release_id"], 53)

    def test_no_args_returns_usage_error(self) -> None:
        out, code = _run_main_and_capture([])
        self.assertEqual(code, 1)
        payload = json.loads(out.strip())
        self.assertFalse(payload["success"])
        self.assertEqual(payload["error_code"], "USAGE_ERROR")

    def test_historical_without_end_date_uses_current_date(self) -> None:
        class _FixedDateTime:
            @staticmethod
            def utcnow():
                return real_datetime(2024, 1, 31)

        expected = {"success": True, "series_id": "GDP", "observations": []}
        with (
            patch.object(fred_data, "datetime", _FixedDateTime),
            patch.object(fred_data, "get_series", return_value=expected) as mock_get_series,
        ):
            _run_main_and_capture(["historical", "GDP", "2020-01-01"])
        mock_get_series.assert_called_once_with("GDP", "2020-01-01", "2024-01-31", None, None)

    def test_malicious_series_token_is_treated_as_plain_argument(self) -> None:
        injection_like = "GDP;rm -rf /"
        expected = {"success": True, "series_id": injection_like, "observations": []}
        with patch.object(fred_data, "get_series", return_value=expected) as mock_get_series:
            out, code = _run_main_and_capture(["series", injection_like])
        self.assertEqual(code, 0)
        mock_get_series.assert_called_once_with(injection_like, None, None, None, None)
        self.assertEqual(json.loads(out.strip())["series_id"], injection_like)

    def test_stdout_contains_single_json_object_on_success(self) -> None:
        expected = {"success": True, "series_id": "UNRATE", "observations": []}
        with patch.object(fred_data, "get_series", return_value=expected):
            out, code = _run_main_and_capture(["series", "UNRATE"])
        self.assertEqual(code, 0)
        lines = [line for line in out.splitlines() if line.strip()]
        self.assertEqual(len(lines), 1)
        payload = json.loads(lines[0])
        self.assertTrue(payload["success"])

    def test_stdout_contains_single_json_object_on_error(self) -> None:
        out, code = _run_main_and_capture(["unknown-cmd"])
        self.assertEqual(code, 1)
        lines = [line for line in out.splitlines() if line.strip()]
        self.assertEqual(len(lines), 1)
        payload = json.loads(lines[0])
        self.assertFalse(payload["success"])
        self.assertEqual(payload["error_code"], "USAGE_ERROR")


if __name__ == "__main__":
    unittest.main()
