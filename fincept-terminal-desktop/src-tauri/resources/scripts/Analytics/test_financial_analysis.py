#!/usr/bin/env python3
"""Test script for financial analysis module - Run from Analytics directory"""
import json
from datetime import date

from finanicalanalysis import (
    DataProcessor, CompanyInfo, FinancialPeriod, ReportingStandard, DataSource,
    IncomeStatementAnalyzer, BalanceSheetAnalyzer, CashFlowAnalyzer,
    ComprehensiveAnalyzer, RiskLevel
)

def main():
    print("="*60)
    print(" FINANCIAL ANALYSIS MODULE TEST")
    print("="*60)
    
    # Create company info
    company = CompanyInfo(
        ticker="AAPL", name="Apple Inc.", sector="Technology",
        industry="Consumer Electronics", country="USA",
        reporting_standard=ReportingStandard.US_GAAP,
        fiscal_year_end="September", presentation_currency="USD"
    )
    
    period = FinancialPeriod(
        period_end=date(2024, 9, 30), period_type="annual",
        fiscal_year=2024, fiscal_period="FY", audit_status="audited"
    )
    
    # Apple-like financials (in millions)
    data = {
        "revenue": 383285, "cost_of_sales": 214137, "operating_income": 114301,
        "net_income": 96995, "interest_expense": 3933, "pretax_income": 113736,
        "tax_expense": 16741, "basic_eps": 6.13, "diluted_eps": 6.08,
        "shares_outstanding_basic": 15820, "shares_outstanding_diluted": 15962,
        "total_assets": 352583, "current_assets": 143566, "cash_equivalents": 29965,
        "accounts_receivable": 60985, "inventory": 6331, "ppe_net": 45336,
        "ppe_gross": 120000, "accumulated_depreciation": 74664,
        "total_liabilities": 290437, "current_liabilities": 145308,
        "long_term_debt": 98071, "short_term_debt": 10985, "total_equity": 62146,
        "retained_earnings": -214, "operating_cash_flow": 110543, "capex": 10959
    }
    
    processor = DataProcessor()
    statements = processor.process_data(data, DataSource.MANUAL, company, period)
    print(f"\n[OK] Created statements for {company.name}")
    
    # Run analyzers
    print("\n--- INCOME STATEMENT ---")
    income = IncomeStatementAnalyzer(enable_logging=False)
    income_results = income.analyze(statements)
    print(f"Generated {len(income_results)} metrics")
    for r in income_results[:3]:
        risk = "LOW" if r.risk_level == RiskLevel.LOW else "MOD" if r.risk_level == RiskLevel.MODERATE else "HIGH"
        print(f"  [{risk}] {r.metric_name}: {r.value:.4f}")
    
    print("\n--- BALANCE SHEET ---")
    balance = BalanceSheetAnalyzer(enable_logging=False)
    balance_results = balance.analyze(statements)
    print(f"Generated {len(balance_results)} metrics")
    for r in balance_results[:3]:
        risk = "LOW" if r.risk_level == RiskLevel.LOW else "MOD" if r.risk_level == RiskLevel.MODERATE else "HIGH"
        print(f"  [{risk}] {r.metric_name}: {r.value:.4f}")
    
    print("\n--- CASH FLOW ---")
    cf = CashFlowAnalyzer(enable_logging=False)
    cf_results = cf.analyze(statements)
    print(f"Generated {len(cf_results)} metrics")
    for r in cf_results[:3]:
        risk = "LOW" if r.risk_level == RiskLevel.LOW else "MOD" if r.risk_level == RiskLevel.MODERATE else "HIGH"
        print(f"  [{risk}] {r.metric_name}: {r.value:.4f}")
    
    print("\n--- COMPREHENSIVE ---")
    comp = ComprehensiveAnalyzer(enable_logging=False)
    all_results = comp.analyze(statements)
    print(f"Total metrics: {len(all_results)}")
    
    # Risk distribution
    low = sum(1 for r in all_results if r.risk_level == RiskLevel.LOW)
    mod = sum(1 for r in all_results if r.risk_level == RiskLevel.MODERATE)
    high = sum(1 for r in all_results if r.risk_level == RiskLevel.HIGH)
    print(f"  LOW: {low}, MODERATE: {mod}, HIGH: {high}")
    
    print("\n" + "="*60)
    print(" ALL TESTS PASSED!")
    print("="*60)

if __name__ == "__main__":
    main()
