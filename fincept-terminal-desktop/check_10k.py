from edgar import Company, set_identity, get_company_facts
import json

set_identity('test test@test.com')

print("=== 10-K DATA EXTRACTION CAPABILITIES ===\n")

# Get Apple as example
c = Company('AAPL')
f = c.latest('10-K')

print("1. BASIC FILING METADATA:")
print(f"   - Accession Number: {f.accession_number}")
print(f"   - Filing Date: {f.filing_date}")
print(f"   - Period of Report: {f.period_of_report}")
print(f"   - Company: {f.company}")
print(f"   - CIK: {f.cik}")
print(f"   - Form: {f.form}")
print(f"   - Is XBRL: {f.is_xbrl}")
print(f"   - Is Inline XBRL: {f.is_inline_xbrl}")

print("\n2. DOCUMENTS & EXHIBITS:")
print(f"   - Primary Document: {f.primary_document}")
docs = f.docs
try:
    print(f"   - Total Documents: {len(list(docs)) if docs else 0}")
except:
    print(f"   - Documents: Available")
exhibits = f.exhibits
try:
    print(f"   - Total Exhibits: {len(list(exhibits)) if exhibits else 0}")
except:
    print(f"   - Exhibits: Available")

print("\n3. FINANCIAL STATEMENTS (via Financials):")
fins = c.get_financials()
print("   Available statements:")
print(f"   - Balance Sheet: {fins.balance_sheet is not None}")
print(f"   - Income Statement: {fins.income_statement is not None}")
print(f"   - Cash Flow Statement: {fins.cashflow_statement is not None}")
print(f"   - Statement of Equity: {fins.statement_of_equity is not None}")
print(f"   - Comprehensive Income: {fins.comprehensive_income is not None}")

print("\n4. FINANCIAL METRICS (built-in methods):")
print("   - Revenue")
print("   - Net Income")
print("   - Operating Cash Flow")
print("   - Free Cash Flow")
print("   - Total Assets")
print("   - Current Assets")
print("   - Total Liabilities")
print("   - Current Liabilities")
print("   - Stockholders Equity")
print("   - Capital Expenditures")

print("\n5. XBRL STATEMENTS (all available):")
x = f.xbrl()
stmts = x.statements
try:
    stmts_list = list(stmts) if stmts else []
    print(f"   Total statements: {len(stmts_list)}")
    for i, stmt in enumerate(stmts_list[:15]):
        name = stmt.name if hasattr(stmt, 'name') else 'Unknown'
        print(f"   {i+1}. {name}")
except Exception as e:
    print(f"   Statements: Available (error iterating: {e})")

print("\n6. XBRL DATA ACCESS:")
print(f"   - Facts: {len(x.facts) if x.facts else 0} facts available")
print(f"   - Contexts: {len(x.contexts) if x.contexts else 0}")
print(f"   - Reporting Periods: {len(x.reporting_periods)}")
print(f"   - Document Type: {x.document_type}")
print(f"   - Entity Name: {x.entity_name}")

print("\n7. COMPANY FACTS API (comprehensive):")
print("   Access to ALL XBRL concepts filed by company:")
print("   - dei: Document and Entity Information")
print("   - us-gaap: US GAAP taxonomy (revenue, assets, etc.)")
print("   - ifrs-full: IFRS taxonomy")
print("   - Custom: Company-specific extensions")

print("\n8. TEXT EXTRACTION:")
print("   - Full text submission")
print("   - HTML content")
print("   - Markdown conversion")
print("   - Section parsing (if supported)")

print("\n9. ATTACHMENTS & RELATED:")
print(f"   - Attachments: {f.attachments is not None}")
print(f"   - Related filings: {f.related_filings is not None}")

print("\n10. WHAT CAN BE EXTRACTED FROM 10-K:")
print("\n=== STRUCTURED DATA (XBRL) ===")
categories = [
    "Financial Position (Balance Sheet)",
    "  - Assets (Current, Non-current, Total)",
    "  - Liabilities (Current, Long-term, Total)",
    "  - Equity (Common stock, Retained earnings, etc.)",
    "  - Working Capital components",
    "",
    "Operations (Income Statement)",
    "  - Revenue/Sales",
    "  - Cost of Revenue",
    "  - Gross Profit",
    "  - Operating Expenses (R&D, SG&A)",
    "  - Operating Income",
    "  - Interest Expense/Income",
    "  - Tax Expense",
    "  - Net Income (continuing/discontinued ops)",
    "  - EPS (Basic & Diluted)",
    "",
    "Cash Flows",
    "  - Operating activities",
    "  - Investing activities",
    "  - Financing activities",
    "  - Free Cash Flow",
    "",
    "Equity Changes",
    "  - Stock issuance/repurchases",
    "  - Dividends",
    "  - Other comprehensive income",
    "",
    "Segment Information",
    "  - Geographic segments",
    "  - Business segments",
    "  - Product line segments",
    "",
    "Detailed Line Items (1000+ concepts)",
    "  - Inventory",
    "  - Property, Plant & Equipment",
    "  - Intangible Assets/Goodwill",
    "  - Debt (short/long term)",
    "  - Deferred revenue",
    "  - Share-based compensation",
    "  - Pension/retirement obligations",
    "  - And many more...",
]

for cat in categories:
    print(f"   {cat}")

print("\n=== TEXTUAL/NARRATIVE DATA ===")
text_sections = [
    "Item 1: Business Description",
    "  - Business overview",
    "  - Products/services",
    "  - Competition",
    "  - Customers (major revenue concentration)",
    "  - Suppliers",
    "",
    "Item 1A: Risk Factors",
    "  - Business risks",
    "  - Market risks",
    "  - Legal/regulatory risks",
    "",
    "Item 2: Properties",
    "  - Real estate holdings",
    "  - Facilities",
    "",
    "Item 3: Legal Proceedings",
    "  - Ongoing litigation",
    "  - Regulatory matters",
    "",
    "Item 4: Mine Safety (if applicable)",
    "",
    "Item 5: Market for Stock",
    "  - Stock performance",
    "  - Dividends",
    "",
    "Item 6: Selected Financial Data",
    "",
    "Item 7: MD&A (Management Discussion & Analysis)",
    "  - Business results",
    "  - Trends",
    "  - Liquidity analysis",
    "  - Capital resources",
    "",
    "Item 7A: Quantitative/Qualitative Market Risk",
    "",
    "Item 8: Financial Statements & Notes",
    "  - Complete audited financials",
    "  - Footnotes/disclosures",
    "",
    "Item 9: Accounting Disagreements",
    "",
    "Item 9A: Controls & Procedures",
    "",
    "Item 10: Directors & Officers (or by reference to proxy)",
    "",
    "Item 11: Executive Compensation (or by reference)",
    "",
    "Item 12: Security Ownership (or by reference)",
    "",
    "Item 13: Related Party Transactions (or by reference)",
    "",
    "Item 14: Principal Accountant Fees",
    "",
    "Item 15: Exhibits & Financial Statement Schedules",
]

for section in text_sections:
    print(f"   {section}")

print("\n=== EXHIBITS ===")
exhibit_types = [
    "3.1/3.2: Articles of Incorporation/Bylaws",
    "4.x: Instruments defining rights of security holders",
    "10.x: Material contracts",
    "21: Subsidiaries",
    "23: Auditor consent",
    "31: CEO/CFO certifications (SOX 302)",
    "32: CEO/CFO certifications (SOX 906)",
    "101: XBRL instance documents",
]
for ex in exhibit_types:
    print(f"   {ex}")

print("\n=== ADVANCED EXTRACTIONS (with parsing) ===")
advanced = [
    "Supply Chain Relationships",
    "  - Major customers (>10% revenue)",
    "  - Key suppliers",
    "  - Strategic partners",
    "",
    "Debt Structure",
    "  - Credit facilities",
    "  - Bond issuances",
    "  - Lenders/creditors",
    "  - Covenants",
    "",
    "Related Party Transactions",
    "",
    "Subsequent Events",
    "",
    "Geographic/Segment Breakdown",
    "  - Revenue by region",
    "  - Assets by region",
    "  - Revenue by product line",
    "",
    "Employee Information",
    "  - Headcount",
    "  - Labor relations",
]
for item in advanced:
    print(f"   {item}")

print("\n" + "="*60)
print("NOTE: Our edgar_tools.py wrapper already extracts:")
print("  ✓ Financial statements (BS, IS, CF)")
print("  ✓ Company info & metadata")
print("  ✓ Supply chain (via 10-K parsing)")
print("  ✓ Segment financials (XBRL dimensional)")
print("  ✓ Company facts (XBRL API)")
print("="*60)
