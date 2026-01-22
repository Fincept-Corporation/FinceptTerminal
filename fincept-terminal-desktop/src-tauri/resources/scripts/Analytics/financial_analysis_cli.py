#!/usr/bin/env python3
"""
Financial Analysis CLI Wrapper
==============================
Command-line interface for the financial analysis module.
Called by Tauri/Rust via worker pool or subprocess.

Usage:
    python financial_analysis_cli.py <command> [args...]

Commands:
    analyze_income <json_data>      - Income statement analysis
    analyze_balance <json_data>     - Balance sheet analysis
    analyze_cashflow <json_data>    - Cash flow analysis
    analyze_comprehensive <json_data> - Full comprehensive analysis
    get_key_metrics <json_data>     - Quick key metrics summary
"""
import sys
import json
import os
from datetime import date

# Add parent directory to path for imports when running via PyO3
script_dir = os.path.dirname(os.path.abspath(__file__)) if '__file__' in dir() else os.getcwd()
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

from finanicalanalysis import (
    DataProcessor, CompanyInfo, FinancialPeriod, ReportingStandard, DataSource,
    IncomeStatementAnalyzer, BalanceSheetAnalyzer, CashFlowAnalyzer,
    ComprehensiveAnalyzer, RiskLevel, AnalysisType
)


def parse_financial_data(json_str: str):
    """Parse JSON input and create FinancialStatements object"""
    data = json.loads(json_str)
    
    # Extract company info
    company_data = data.get("company", {})
    company = CompanyInfo(
        ticker=company_data.get("ticker", "UNKNOWN"),
        name=company_data.get("name", "Unknown Company"),
        sector=company_data.get("sector", "Unknown"),
        industry=company_data.get("industry", "Unknown"),
        country=company_data.get("country", "USA"),
        reporting_standard=ReportingStandard.US_GAAP,
        fiscal_year_end=company_data.get("fiscal_year_end", "December"),
        presentation_currency=company_data.get("currency", "USD")
    )
    
    # Extract period info
    period_data = data.get("period", {})
    period = FinancialPeriod(
        period_end=date.fromisoformat(period_data.get("period_end", "2024-12-31")),
        period_type=period_data.get("period_type", "annual"),
        fiscal_year=period_data.get("fiscal_year", 2024),
        fiscal_period=period_data.get("fiscal_period", "FY"),
        audit_status=period_data.get("audit_status", "unaudited")
    )
    
    # Extract financial data
    financials = data.get("financials", {})
    
    processor = DataProcessor()
    return processor.process_data(financials, DataSource.MANUAL, company, period)


def results_to_json(results: list) -> str:
    """Convert analysis results to JSON"""
    output = []
    for r in results:
        output.append({
            "metric_name": r.metric_name,
            "value": round(r.value, 6) if isinstance(r.value, float) else r.value,
            "risk_level": r.risk_level.value,
            "analysis_type": r.analysis_type.value,
            "interpretation": r.interpretation,
            "methodology": r.methodology if hasattr(r, "methodology") and r.methodology else None,
            "benchmark_comparison": r.benchmark_comparison if hasattr(r, "benchmark_comparison") else None,
            "limitations": r.limitations if hasattr(r, "limitations") and r.limitations else None,
        })
    
    # Create summary from first few results
    summary = "Analysis complete. "
    if output:
        key_findings = [f"{r['metric_name']}: {r['interpretation'][:50]}..." for r in output[:3]]
        summary += " | ".join(key_findings)
    
    return json.dumps({"success": True, "results": output, "summary": summary}, indent=2)


def analyze_income(json_data: str) -> str:
    """Analyze income statement"""
    statements = parse_financial_data(json_data)
    analyzer = IncomeStatementAnalyzer(enable_logging=False)
    results = analyzer.analyze(statements)
    return results_to_json(results)


def analyze_balance(json_data: str) -> str:
    """Analyze balance sheet"""
    statements = parse_financial_data(json_data)
    analyzer = BalanceSheetAnalyzer(enable_logging=False)
    results = analyzer.analyze(statements)
    return results_to_json(results)


def analyze_cashflow(json_data: str) -> str:
    """Analyze cash flow statement"""
    statements = parse_financial_data(json_data)
    analyzer = CashFlowAnalyzer(enable_logging=False)
    results = analyzer.analyze(statements)
    return results_to_json(results)


def analyze_comprehensive(json_data: str) -> str:
    """Run comprehensive analysis across all statements"""
    statements = parse_financial_data(json_data)
    analyzer = ComprehensiveAnalyzer(enable_logging=False)
    results = analyzer.analyze(statements)
    return results_to_json(results)


def get_key_metrics(json_data: str) -> str:
    """Get quick key metrics summary"""
    statements = parse_financial_data(json_data)
    
    # Get key metrics from each analyzer
    income_analyzer = IncomeStatementAnalyzer(enable_logging=False)
    balance_analyzer = BalanceSheetAnalyzer(enable_logging=False)
    
    income_metrics = income_analyzer.get_key_metrics(statements)
    balance_metrics = balance_analyzer.get_key_metrics(statements)
    
    # Combine metrics
    all_metrics = {**income_metrics, **balance_metrics}
    
    summary = f"Key metrics for {statements.company_info.ticker}: "
    metric_strs = [f"{k}={v:.2f}" if isinstance(v, float) else f"{k}={v}" for k, v in list(all_metrics.items())[:5]]
    summary += ", ".join(metric_strs)
    
    return json.dumps({
        "success": True,
        "company": statements.company_info.ticker,
        "period": str(statements.period_info.period_end),
        "metrics": {k: round(v, 6) if isinstance(v, float) else v for k, v in all_metrics.items()},
        "summary": summary
    }, indent=2)


def main(args: list) -> str:
    """Main entry point for PyO3 execution"""
    if len(args) < 2:
        return json.dumps({
            "success": False,
            "error": "Usage: <command> <json_data>",
            "commands": ["analyze_income", "analyze_balance", "analyze_cashflow", "analyze_comprehensive", "get_key_metrics"]
        })
    
    command = args[0]
    json_data = args[1]
    
    try:
        if command == "analyze_income":
            return analyze_income(json_data)
        elif command == "analyze_balance":
            return analyze_balance(json_data)
        elif command == "analyze_cashflow":
            return analyze_cashflow(json_data)
        elif command == "analyze_comprehensive":
            return analyze_comprehensive(json_data)
        elif command == "get_key_metrics":
            return get_key_metrics(json_data)
        else:
            return json.dumps({"success": False, "error": f"Unknown command: {command}"})
    except Exception as e:
        import traceback
        return json.dumps({"success": False, "error": str(e), "traceback": traceback.format_exc()})


if __name__ == "__main__":
    result = main(sys.argv[1:])
    print(result)
