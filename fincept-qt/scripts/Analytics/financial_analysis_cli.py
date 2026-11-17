#!/usr/bin/env python3
"""
Financial Statement Analysis CLI
================================

    python Analytics/financial_analysis_cli.py <command> '<json params>'
    python Analytics/financial_analysis_cli.py <command> -        (params JSON on stdin)
    python Analytics/financial_analysis_cli.py list

Prints exactly one JSON object on stdout (NaN/Infinity become null). On failure:
{"success": false, "error": "<plain message>"}. Diagnostics go to stderr.

PARAMS
  company   {"ticker","name","sector","industry","country","currency",
             "reporting_standard": "US_GAAP" | "IFRS" | "LOCAL_GAAP"}
  period    {"period_end": "YYYY-MM-DD" | "FY2024", "fiscal_year", "period_type",
             "audit_status"}
  financials  one period of statements, in any of these shapes:
             - flat line items {"revenue": 1000, "net_income": 100, ...}
             - per-statement sections {"income_statement": {...}, "balance_sheet": {...},
               "cash_flow": {...}, "notes": {...}}
             - yfinance_data.py "financials" output (sections keyed by period date):
               the latest period is analysed, earlier ones become history
             Values may be numbers or numeric strings ("1,234", "(56)"); null / "N/A"
             means not reported. Row names are matched case- and space-insensitively
             (yfinance names such as "Total Revenue" or "Net PPE" are recognised).
  history   optional [{"period": {...}, "financials": {...}}, ...], OLDEST FIRST.
            Enables growth rates, trends, average-balance ratios and Beneish M-score.
  industry  optional benchmarks {"gross_margin": {"median","q1","q3"}, ...}
  notes     optional note disclosures (lifo_reserve, statutory_tax_rate, ...)

CONVENTIONS (see finanicalanalysis/core/data_processor.py)
  * Missing is not zero: a ratio whose inputs are not reported is returned with
    "value": null and an interpretation starting "Not available:".
  * Cash-flow outflows (capex, dividends_paid, debt_repaid, equity_repurchased,
    acquisitions, ...) may be supplied with either sign; they are normalised to
    positive magnitudes. Subtotals (operating/investing/financing cash flow) keep
    their sign. Working-capital change lines use the cash-flow-statement sign.
  * Ratios mixing a flow with a balance use the average of opening and closing
    balances when history is supplied, otherwise the ending balance (stated in
    each result's methodology).
"""
import os
import sys
import json
import re
import traceback
from datetime import date, datetime

script_dir = os.path.dirname(os.path.abspath(__file__))
scripts_root = os.path.dirname(script_dir)
for _p in (script_dir, scripts_root):
    if _p not in sys.path:
        sys.path.insert(0, _p)

import fincept_json  # noqa: E402  (shared NaN-safe output boundary)

from finanicalanalysis import (  # noqa: E402
    DataProcessor, CompanyInfo, FinancialPeriod, ReportingStandard, DataSource,
    IncomeStatementAnalyzer, BalanceSheetAnalyzer, CashFlowAnalyzer, ComprehensiveAnalyzer,
    InventoryAnalyzer, LongTermAssetAnalyzer, TaxAnalyzer, FinancialReportingQualityAnalyzer,
)
from finanicalanalysis.core.data_processor import SECTION_ALIASES, normalize_key  # noqa: E402


class CliError(Exception):
    """An input problem reported to the caller as {"error": ...}."""


# --------------------------------------------------------------------- parsing

_STANDARDS = {
    'us_gaap': ReportingStandard.US_GAAP, 'usgaap': ReportingStandard.US_GAAP, 'gaap': ReportingStandard.US_GAAP,
    'ifrs': ReportingStandard.IFRS, 'local_gaap': ReportingStandard.LOCAL_GAAP, 'local': ReportingStandard.LOCAL_GAAP,
}
_YF_METADATA_KEYS = {'symbol', 'row_order', 'quarterly', 'ttm', 'currency', 'error'}


def _company(data: dict) -> CompanyInfo:
    c = data.get("company") or {}
    if not isinstance(c, dict):
        raise CliError("'company' must be an object")
    std_raw = normalize_key(c.get("reporting_standard", "US_GAAP") or "US_GAAP")
    if std_raw not in _STANDARDS:
        raise CliError(f"unknown reporting_standard {c.get('reporting_standard')!r} (use US_GAAP, IFRS or LOCAL_GAAP)")
    fin = data.get("financials")
    fin_symbol = fin.get("symbol") if isinstance(fin, dict) and isinstance(fin.get("symbol"), str) else None
    return CompanyInfo(
        ticker=str(c.get("ticker") or c.get("symbol") or fin_symbol or "UNKNOWN"),
        name=str(c.get("name", "Unknown Company")),
        sector=str(c.get("sector", "Unknown")),
        industry=str(c.get("industry", "Unknown")),
        country=str(c.get("country", "Unknown")),
        reporting_standard=_STANDARDS[std_raw],
        fiscal_year_end=str(c.get("fiscal_year_end", "December")),
        presentation_currency=str(c.get("currency", "USD")),
    )


def _parse_date(value):
    """'2024-12-31', '2024-12-31 00:00:00', 'FY2024', 2024 -> (date or None, fiscal_year or None)."""
    if value is None or value == "":
        return None, None
    s = str(value).strip()
    try:
        d = date.fromisoformat(s[:10])
        return d, d.year
    except ValueError:
        pass
    m = re.search(r'(19|20)\d{2}', s)
    if m:
        fy = int(m.group(0))
        return date(fy, 12, 31), fy
    raise CliError(f"cannot read period_end {value!r}; use YYYY-MM-DD or FY2024")


def _period(period_data, fallback_key=None) -> FinancialPeriod:
    p = period_data or {}
    if not isinstance(p, dict):
        raise CliError("'period' must be an object")
    period_end, fy = _parse_date(p.get("period_end") or fallback_key)
    if p.get("fiscal_year") is not None:
        try:
            fy = int(p["fiscal_year"])
        except (TypeError, ValueError):
            raise CliError(f"fiscal_year {p['fiscal_year']!r} is not a year")
        if period_end is None:
            period_end = date(fy, 12, 31)
    return FinancialPeriod(
        period_end=period_end,
        period_type=str(p.get("period_type", "annual")),
        fiscal_year=fy,
        fiscal_period=str(p.get("fiscal_period", "FY")),
        audit_status=p.get("audit_status"),
    )


def _is_multi_period(financials: dict) -> bool:
    """yfinance shape: every statement section maps period keys to dicts of line items."""
    sections = [v for k, v in financials.items() if normalize_key(k) in SECTION_ALIASES and isinstance(v, dict)]
    return bool(sections) and all(v and all(isinstance(x, dict) for x in v.values()) for v in sections)


def _split_multi_period(financials: dict):
    """-> list of (period_key, single-period financials), oldest first."""
    per_period = {}
    for key, section in financials.items():
        nk = normalize_key(key)
        if nk not in SECTION_ALIASES or not isinstance(section, dict):
            continue
        for period_key, items in section.items():
            per_period.setdefault(str(period_key), {})[key] = items

    def sort_key(k):
        try:
            return (0, _parse_date(k)[0] or date.min, k)
        except CliError:
            return (1, date.min, k)

    return [(k, per_period[k]) for k in sorted(per_period, key=sort_key)]


def _process(processor, company, period, financials, notes=None):
    if not isinstance(financials, dict) or not financials:
        raise CliError("'financials' must be a non-empty object of statement line items")
    payload = {k: v for k, v in financials.items()
               if not (normalize_key(k) in _YF_METADATA_KEYS)}
    if notes:
        if not isinstance(notes, dict):
            raise CliError("'notes' must be an object")
        payload = dict(payload)
        payload["notes"] = {**(payload.get("notes") or {}), **notes}
    try:
        return processor.process_data(payload, DataSource.MANUAL, company, period)
    except ValueError as e:
        raise CliError(str(e))


def parse_request(json_str: str):
    """-> (current FinancialStatements, history list oldest first, industry dict, raw params)."""
    try:
        data = json.loads(json_str)
    except json.JSONDecodeError as e:
        raise CliError(f"params are not valid JSON: {e}")
    if not isinstance(data, dict):
        raise CliError("params must be a JSON object")

    company = _company(data)
    financials = data.get("financials")
    if financials is None:
        raise CliError("missing 'financials'")
    if not isinstance(financials, dict):
        raise CliError("'financials' must be an object")
    processor = DataProcessor()
    notes = data.get("notes")

    history = []
    if _is_multi_period(financials):
        periods = _split_multi_period(financials)
        cur_key, cur_fin = periods[-1]
        current = _process(processor, company, _period(data.get("period"), cur_key), cur_fin, notes)
        if not data.get("history"):
            for key, fin in periods[:-1]:
                history.append(_process(processor, company, _period(None, key), fin))
    else:
        current = _process(processor, company, _period(data.get("period")), financials, notes)

    raw_history = data.get("history") or []
    if not isinstance(raw_history, list):
        raise CliError("'history' must be a list of {period, financials}, oldest first")
    for i, h in enumerate(raw_history):
        if not isinstance(h, dict) or not isinstance(h.get("financials"), dict):
            raise CliError(f"history[{i}] must be an object with a 'financials' object")
        history.append(_process(processor, company, _period(h.get("period")), h["financials"], h.get("notes")))

    core = ('revenue', 'net_income', 'total_assets', 'total_equity', 'operating_cash_flow')
    present = [k for k in core if k in current.income_statement or k in current.balance_sheet
               or k in current.cash_flow]
    if not present:
        recognised = sorted(set(current.income_statement) | set(current.balance_sheet) | set(current.cash_flow))
        raise CliError("no recognised core statement fields (need at least one of "
                       f"{', '.join(core)}); recognised: {recognised or 'none'}")

    industry = data.get("industry") or None
    if industry is not None and not isinstance(industry, dict):
        raise CliError("'industry' must be an object")
    return current, history, industry, data


def parse_financial_data(json_str: str):
    """Backward-compatible helper: the current-period FinancialStatements only."""
    return parse_request(json_str)[0]


# --------------------------------------------------------------------- output

def _round(v):
    return round(v, 6) if isinstance(v, float) else v


def _result_dict(r) -> dict:
    return {
        "metric_name": r.metric_name,
        "value": _round(r.value),
        "classification": r.label,
        "risk_level": r.risk_level.value if r.risk_level is not None else None,
        "analysis_type": r.analysis_type.value,
        "interpretation": r.interpretation,
        "methodology": r.methodology or None,
        "benchmark_comparison": r.benchmark_comparison,
        "limitations": r.limitations or None,
        "recommendations": r.recommendations or None,
        "trend": r.trend.value if r.trend is not None else None,
        "quality_score": _round(r.quality_score),
    }


def _data_quality(statements, history) -> dict:
    dq = statements.data_quality
    out = {
        "validation_warnings": dq.get("validation_warnings", []),
        "missing_core_fields": dq.get("missing_fields", []),
        "sign_normalised": dq.get("sign_normalised", []),
        "values_ignored": dq.get("ignored_values", [])[:20],
        "history_periods": len(history),
    }
    if statements.notes:
        out["notes_keys"] = sorted(statements.notes)[:50]
    return out


def _period_str(statements):
    pe = statements.period_info.period_end
    return str(pe) if pe else None


def results_payload(command, statements, history, results) -> dict:
    output = [_result_dict(r) for r in results]
    assessed = [r for r in output if r["value"] is not None or r["classification"] is not None]
    n_na = len(output) - len(assessed)
    findings = [f"{r['metric_name']}: {r['interpretation'][:60]}" for r in assessed[:3]]
    summary = (f"{len(assessed)} metrics computed, {n_na} not available from the supplied data. "
               + " | ".join(findings))
    return {
        "success": True,
        "command": command,
        "company": statements.company_info.ticker,
        "period": _period_str(statements),
        "results": output,
        "summary": summary,
        "data_quality": _data_quality(statements, history),
    }


def results_to_json(results: list) -> str:
    """Backward-compatible helper: results list -> NaN-safe one-line JSON string."""
    return fincept_json.dumps({"success": True, "results": [_result_dict(r) for r in results]})


# -------------------------------------------------------------------- commands

def _analyzer_command(cls):
    def run(command, json_data):
        statements, history, industry, _ = parse_request(json_data)
        results = cls(enable_logging=False).analyze(statements, history or None, industry)
        return results_payload(command, statements, history, results)
    return run


def get_key_metrics(command, json_data):
    statements, history, _, _ = parse_request(json_data)
    unavailable = {}
    metrics = ComprehensiveAnalyzer(enable_logging=False).get_key_metrics(statements, history or None, unavailable)
    for k in metrics:
        unavailable.pop(k, None)
    shown = list(metrics.items())[:5]
    summary = f"Key metrics for {statements.company_info.ticker}: " + ", ".join(
        f"{k}={v:.4g}" if isinstance(v, float) else f"{k}={v}" for k, v in shown)
    return {
        "success": True,
        "command": command,
        "company": statements.company_info.ticker,
        "period": _period_str(statements),
        "metrics": {k: _round(v) for k, v in metrics.items()},
        "unavailable": unavailable,
        "basis": ("average balances (history supplied)" if history
                  else "ending balances (no history supplied)"),
        "definitions": {
            "free_cash_flow_firm": "operating cash flow - capital expenditure (name kept for compatibility; not FCFF)",
            "quick_ratio": "(cash + short-term investments + receivables) / current liabilities",
            "cash_ratio": "(cash + short-term investments) / current liabilities",
            "effective_tax_rate": "tax expense / pre-tax income, only when pre-tax income is positive",
        },
        "summary": summary,
        "data_quality": _data_quality(statements, history),
    }


def build_model(command, json_data):
    statements, history, _, data = parse_request(json_data)
    from finanicalanalysis.specialized_analysis.financial_modeling import FinancialStatementModelingAnalyzer
    years = data.get("forecast_years", 5)
    try:
        years = int(years)
    except (TypeError, ValueError):
        raise CliError("forecast_years must be an integer")
    if not 1 <= years <= 30:
        raise CliError("forecast_years must be between 1 and 30")
    assumptions = data.get("assumptions") or {}
    if not isinstance(assumptions, dict):
        raise CliError("'assumptions' must be an object")
    analyzer = FinancialStatementModelingAnalyzer()
    try:
        model = analyzer.build_integrated_model(statements, statements.company_info.name, years,
                                                assumptions, comparative_data=history or None)
    except ValueError as e:
        raise CliError(str(e))
    out = analyzer.model_to_dict(model)
    sens = data.get("sensitivity")
    if sens:
        if not isinstance(sens, dict) or "variable" not in sens:
            raise CliError("'sensitivity' must be an object with a 'variable' (an assumption name)")
        try:
            s = analyzer.perform_sensitivity_analysis(model, sens["variable"],
                                                      float(sens.get("range_percent", 0.2)),
                                                      int(sens.get("num_steps", 5)))
        except ValueError as e:
            raise CliError(str(e))
        out["sensitivity"] = analyzer.sensitivity_to_dict(s)
    out.update({"success": True, "command": command, "company": statements.company_info.ticker,
                "period": _period_str(statements),
                "data_quality": _data_quality(statements, history)})
    return out


COMMANDS = {
    "analyze_income": (_analyzer_command(IncomeStatementAnalyzer),
                       "Profitability, EPS, revenue/expense quality, accruals, common-size income statement"),
    "analyze_balance": (_analyzer_command(BalanceSheetAnalyzer),
                        "Liquidity, solvency, asset composition, equity and returns"),
    "analyze_cashflow": (_analyzer_command(CashFlowAnalyzer),
                         "Operating/investing/financing cash flow, FCF, FCFE, CFA coverage ratios"),
    "analyze_comprehensive": (_analyzer_command(ComprehensiveAnalyzer),
                              "Composite scores, business model, statement linkages, risk assessment"),
    "get_key_metrics": (get_key_metrics, "Flat dictionary of key ratios plus reasons for any that are unavailable"),
    "analyze_quality": (_analyzer_command(FinancialReportingQualityAnalyzer),
                        "Earnings quality, accruals, Beneish M-score (needs history), red flags"),
    "analyze_assets": (_analyzer_command(LongTermAssetAnalyzer),
                       "PPE age and turnover, capex vs depreciation, intangibles, goodwill, impairment"),
    "analyze_tax": (_analyzer_command(TaxAnalyzer),
                    "Effective / cash tax rates, deferred taxes, reconciliation (statutory rate from notes or country)"),
    "analyze_inventory": (_analyzer_command(InventoryAnalyzer),
                          "Inventory turnover, DIO, LIFO reserve and FIFO conversion, write-downs"),
    "build_model": (build_model,
                    "Three-statement projection from the supplied statements; optional DCF and sensitivity. "
                    "Params: forecast_years, assumptions{revenue_growth_rate (required without history), "
                    "tax_rate, capex_to_revenue, dso, dio, dpo, dividend_payout, interest_rate, min_cash, "
                    "wacc, terminal_growth, ...}, sensitivity{variable, range_percent, num_steps}"),
}


def list_commands(command=None, json_data=None):
    return {
        "success": True,
        "command": "list",
        "usage": "python Analytics/financial_analysis_cli.py <command> '<json params>'  (or '-' to read params from stdin)",
        "commands": [{"name": name, "description": desc} for name, (_, desc) in COMMANDS.items()],
        "params": ["company", "period", "financials", "history (oldest first)", "industry", "notes"],
    }


def main(args: list) -> dict:
    if not args:
        return {"success": False, "error": "usage: financial_analysis_cli.py <command> '<json params>'; "
                                           "run the 'list' command for the available commands"}
    command = args[0]
    if command == "list":
        return list_commands()
    if command not in COMMANDS:
        return {"success": False, "error": f"unknown command '{command}'; available: list, {', '.join(COMMANDS)}"}

    if len(args) < 2 or args[1] == "-":
        json_data = "" if sys.stdin is None or sys.stdin.isatty() else sys.stdin.read()
    else:
        json_data = args[1]
    if not json_data.strip():
        return {"success": False, "error": f"'{command}' needs a JSON params argument (or '-' with JSON on stdin)"}

    try:
        return COMMANDS[command][0](command, json_data)
    except CliError as e:
        return {"success": False, "error": str(e)}
    except Exception as e:  # unexpected: plain message out, traceback to stderr
        traceback.print_exc(file=sys.stderr)
        return {"success": False, "error": f"{command} failed: {type(e).__name__}: {e}"}


if __name__ == "__main__":
    fincept_json.emit(main(sys.argv[1:]))
