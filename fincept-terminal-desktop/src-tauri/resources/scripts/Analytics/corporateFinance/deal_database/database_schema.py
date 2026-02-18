"""SQLite Database Schema for M&A Deals"""
import sys
import sqlite3
from pathlib import Path
from typing import Optional, Dict, Any, List
from datetime import datetime
import json

# Add Analytics path for absolute imports
scripts_path = Path(__file__).parent.parent.parent.parent
sys.path.append(str(scripts_path))

analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

# Use absolute imports instead of relative imports
from corporateFinance.config import MA_DATABASE as DEFAULT_MA_DATABASE

class MADatabase:
    def __init__(self, db_path: Optional[Path] = None):
        if db_path is None:
            db_path = DEFAULT_MA_DATABASE

        self.db_path = db_path
        self.db_path.parent.mkdir(parents=True, exist_ok=True)
        self.conn = None
        self._initialize_database()

    def _get_connection(self):
        if self.conn is None:
            self.conn = sqlite3.connect(str(self.db_path))
            self.conn.row_factory = sqlite3.Row
        return self.conn

    def _initialize_database(self):
        conn = self._get_connection()
        cursor = conn.cursor()

        cursor.execute("""
        CREATE TABLE IF NOT EXISTS ma_deals (
            deal_id TEXT PRIMARY KEY,
            announcement_date DATE NOT NULL,
            expected_close_date DATE,
            actual_close_date DATE,
            acquirer_name TEXT NOT NULL,
            acquirer_ticker TEXT,
            acquirer_cik TEXT,
            target_name TEXT NOT NULL,
            target_ticker TEXT,
            target_cik TEXT,
            deal_value REAL,
            deal_value_currency TEXT DEFAULT 'USD',
            enterprise_value REAL,
            equity_value REAL,
            deal_type TEXT NOT NULL,
            deal_status TEXT NOT NULL,
            payment_method TEXT,
            cash_percentage REAL,
            stock_percentage REAL,
            industry TEXT,
            sector TEXT,
            sub_sector TEXT,
            deal_rationale TEXT,
            strategic_fit_score REAL,
            synergies_disclosed REAL,
            synergy_type TEXT,
            integration_costs REAL,
            premium_1day REAL,
            premium_1week REAL,
            premium_4week REAL,
            premium_52week_high REAL,
            hostile_flag INTEGER DEFAULT 0,
            tender_offer_flag INTEGER DEFAULT 0,
            cross_border_flag INTEGER DEFAULT 0,
            acquirer_country TEXT,
            target_country TEXT,
            regulatory_concerns TEXT,
            antitrust_review_flag INTEGER DEFAULT 0,
            breakup_fee REAL,
            termination_conditions TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            data_source TEXT,
            filing_url TEXT,
            notes TEXT
        )""")

        cursor.execute("""
        CREATE TABLE IF NOT EXISTS deal_financials (
            financial_id INTEGER PRIMARY KEY AUTOINCREMENT,
            deal_id TEXT NOT NULL,
            period_type TEXT NOT NULL,
            fiscal_year INTEGER,
            fiscal_quarter INTEGER,
            party TEXT NOT NULL,
            revenue REAL,
            ebitda REAL,
            ebit REAL,
            net_income REAL,
            total_assets REAL,
            total_liabilities REAL,
            shareholders_equity REAL,
            cash_and_equivalents REAL,
            total_debt REAL,
            market_cap REAL,
            shares_outstanding REAL,
            book_value_per_share REAL,
            eps REAL,
            operating_cash_flow REAL,
            free_cash_flow REAL,
            capex REAL,
            revenue_growth REAL,
            ebitda_margin REAL,
            net_margin REAL,
            roe REAL,
            roa REAL,
            debt_to_equity REAL,
            current_ratio REAL,
            quick_ratio REAL,
            data_date DATE,
            FOREIGN KEY (deal_id) REFERENCES ma_deals(deal_id)
        )""")

        cursor.execute("""
        CREATE TABLE IF NOT EXISTS deal_multiples (
            multiple_id INTEGER PRIMARY KEY AUTOINCREMENT,
            deal_id TEXT NOT NULL,
            calculation_date DATE NOT NULL,
            ev_revenue REAL,
            ev_ebitda REAL,
            ev_ebit REAL,
            price_earnings REAL,
            price_book REAL,
            price_sales REAL,
            ev_fcf REAL,
            peg_ratio REAL,
            acquirer_ev_revenue REAL,
            acquirer_ev_ebitda REAL,
            acquirer_pe REAL,
            target_ev_revenue REAL,
            target_ev_ebitda REAL,
            target_pe REAL,
            implied_equity_value REAL,
            implied_enterprise_value REAL,
            multiple_type TEXT,
            ltm_or_ntm TEXT,
            FOREIGN KEY (deal_id) REFERENCES ma_deals(deal_id)
        )""")

        cursor.execute("""
        CREATE TABLE IF NOT EXISTS deal_advisors (
            advisor_id INTEGER PRIMARY KEY AUTOINCREMENT,
            deal_id TEXT NOT NULL,
            party TEXT NOT NULL,
            advisor_type TEXT NOT NULL,
            advisor_name TEXT NOT NULL,
            advisor_role TEXT,
            fee_disclosed REAL,
            FOREIGN KEY (deal_id) REFERENCES ma_deals(deal_id)
        )""")

        cursor.execute("""
        CREATE TABLE IF NOT EXISTS deal_timeline (
            timeline_id INTEGER PRIMARY KEY AUTOINCREMENT,
            deal_id TEXT NOT NULL,
            event_date DATE NOT NULL,
            event_type TEXT NOT NULL,
            event_description TEXT,
            milestone_status TEXT,
            FOREIGN KEY (deal_id) REFERENCES ma_deals(deal_id)
        )""")

        cursor.execute("""
        CREATE TABLE IF NOT EXISTS deal_synergies (
            synergy_id INTEGER PRIMARY KEY AUTOINCREMENT,
            deal_id TEXT NOT NULL,
            synergy_category TEXT NOT NULL,
            synergy_type TEXT NOT NULL,
            annual_value REAL,
            one_time_value REAL,
            realization_year INTEGER,
            realization_percentage REAL,
            description TEXT,
            confidence_level TEXT,
            FOREIGN KEY (deal_id) REFERENCES ma_deals(deal_id)
        )""")

        cursor.execute("""
        CREATE TABLE IF NOT EXISTS comparable_companies (
            comp_id INTEGER PRIMARY KEY AUTOINCREMENT,
            company_name TEXT NOT NULL,
            ticker TEXT,
            cik TEXT,
            industry TEXT,
            sector TEXT,
            market_cap REAL,
            enterprise_value REAL,
            revenue REAL,
            ebitda REAL,
            net_income REAL,
            ev_revenue REAL,
            ev_ebitda REAL,
            pe_ratio REAL,
            as_of_date DATE,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )""")

        cursor.execute("""
        CREATE TABLE IF NOT EXISTS industry_averages (
            avg_id INTEGER PRIMARY KEY AUTOINCREMENT,
            industry TEXT NOT NULL,
            sector TEXT,
            calculation_period TEXT,
            period_start DATE,
            period_end DATE,
            avg_ev_revenue REAL,
            median_ev_revenue REAL,
            avg_ev_ebitda REAL,
            median_ev_ebitda REAL,
            avg_pe REAL,
            median_pe REAL,
            avg_premium_1day REAL,
            median_premium_1day REAL,
            avg_premium_4week REAL,
            median_premium_4week REAL,
            deal_count INTEGER,
            total_deal_value REAL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )""")

        cursor.execute("""
        CREATE TABLE IF NOT EXISTS sec_filings (
            filing_id INTEGER PRIMARY KEY AUTOINCREMENT,
            deal_id TEXT,
            accession_number TEXT UNIQUE NOT NULL,
            filing_type TEXT NOT NULL,
            filing_date DATE NOT NULL,
            company_name TEXT,
            cik TEXT,
            items TEXT,
            filing_url TEXT,
            html_url TEXT,
            parsed_flag INTEGER DEFAULT 0,
            contains_ma_flag INTEGER DEFAULT 0,
            confidence_score REAL,
            raw_text TEXT,
            extracted_data TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (deal_id) REFERENCES ma_deals(deal_id)
        )""")

        cursor.execute("CREATE INDEX IF NOT EXISTS idx_deals_announcement ON ma_deals(announcement_date)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_deals_status ON ma_deals(deal_status)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_deals_industry ON ma_deals(industry)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_deals_acquirer ON ma_deals(acquirer_ticker)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_deals_target ON ma_deals(target_ticker)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_deals_value ON ma_deals(deal_value)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_financials_deal ON deal_financials(deal_id)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_multiples_deal ON deal_multiples(deal_id)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_timeline_deal ON deal_timeline(deal_id)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_filings_accession ON sec_filings(accession_number)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_filings_type ON sec_filings(filing_type)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_filings_date ON sec_filings(filing_date)")

        conn.commit()

    def insert_deal(self, deal_data: Dict[str, Any]) -> str:
        conn = self._get_connection()
        cursor = conn.cursor()

        deal_data['updated_at'] = datetime.now().isoformat()

        columns = ', '.join(deal_data.keys())
        placeholders = ', '.join(['?' for _ in deal_data])

        cursor.execute(f"""
            INSERT OR REPLACE INTO ma_deals ({columns})
            VALUES ({placeholders})
        """, list(deal_data.values()))

        conn.commit()
        return deal_data.get('deal_id')

    def insert_financials(self, financial_data: Dict[str, Any]) -> int:
        conn = self._get_connection()
        cursor = conn.cursor()

        columns = ', '.join(financial_data.keys())
        placeholders = ', '.join(['?' for _ in financial_data])

        cursor.execute(f"""
            INSERT INTO deal_financials ({columns})
            VALUES ({placeholders})
        """, list(financial_data.values()))

        conn.commit()
        return cursor.lastrowid

    def insert_multiples(self, multiples_data: Dict[str, Any]) -> int:
        conn = self._get_connection()
        cursor = conn.cursor()

        columns = ', '.join(multiples_data.keys())
        placeholders = ', '.join(['?' for _ in multiples_data])

        cursor.execute(f"""
            INSERT INTO deal_multiples ({columns})
            VALUES ({placeholders})
        """, list(multiples_data.values()))

        conn.commit()
        return cursor.lastrowid

    def insert_sec_filing(self, filing_data: Dict[str, Any]) -> int:
        conn = self._get_connection()
        cursor = conn.cursor()

        columns = ', '.join(filing_data.keys())
        placeholders = ', '.join(['?' for _ in filing_data])

        cursor.execute(f"""
            INSERT OR IGNORE INTO sec_filings ({columns})
            VALUES ({placeholders})
        """, list(filing_data.values()))

        conn.commit()
        return cursor.lastrowid

    def search_deals(self, filters: Dict[str, Any]) -> List[Dict[str, Any]]:
        conn = self._get_connection()
        cursor = conn.cursor()

        where_clauses = []
        params = []

        # Free-text query search across key text fields
        if 'query' in filters and filters['query']:
            query_val = f"%{filters['query']}%"
            where_clauses.append(
                "(target_name LIKE ? OR acquirer_name LIKE ? OR industry LIKE ? "
                "OR target_ticker LIKE ? OR acquirer_ticker LIKE ? OR deal_id LIKE ?)"
            )
            params.extend([query_val] * 6)

        if 'industry' in filters:
            where_clauses.append("industry = ?")
            params.append(filters['industry'])

        if 'min_value' in filters:
            where_clauses.append("deal_value >= ?")
            params.append(filters['min_value'])

        if 'max_value' in filters:
            where_clauses.append("deal_value <= ?")
            params.append(filters['max_value'])

        if 'start_date' in filters:
            where_clauses.append("announcement_date >= ?")
            params.append(filters['start_date'])

        if 'end_date' in filters:
            where_clauses.append("announcement_date <= ?")
            params.append(filters['end_date'])

        if 'deal_status' in filters:
            where_clauses.append("deal_status = ?")
            params.append(filters['deal_status'])

        if 'acquirer_ticker' in filters:
            where_clauses.append("acquirer_ticker = ?")
            params.append(filters['acquirer_ticker'])

        if 'target_ticker' in filters:
            where_clauses.append("target_ticker = ?")
            params.append(filters['target_ticker'])

        where_sql = " AND ".join(where_clauses) if where_clauses else "1=1"

        query = f"""
            SELECT * FROM ma_deals
            WHERE {where_sql}
            ORDER BY announcement_date DESC
        """

        cursor.execute(query, params)
        return [dict(row) for row in cursor.fetchall()]

    def get_all_deals(self, filters: Optional[Dict[str, Any]] = None) -> List[Dict[str, Any]]:
        """Get all deals with optional filters"""
        if filters:
            return self.search_deals(filters)
        else:
            conn = self._get_connection()
            cursor = conn.cursor()
            cursor.execute("SELECT * FROM ma_deals ORDER BY announcement_date DESC")
            return [dict(row) for row in cursor.fetchall()]

    def update_deal(self, deal_id: str, updates: Dict[str, Any]) -> None:
        """Update an existing deal"""
        conn = self._get_connection()
        cursor = conn.cursor()

        # Build SET clause dynamically from updates dict
        set_clauses = []
        params = []

        for key, value in updates.items():
            set_clauses.append(f"{key} = ?")
            params.append(value)

        params.append(deal_id)  # For WHERE clause

        if set_clauses:
            query = f"UPDATE ma_deals SET {', '.join(set_clauses)} WHERE deal_id = ?"
            cursor.execute(query, params)
            conn.commit()

    def get_deal_by_id(self, deal_id: str) -> Optional[Dict[str, Any]]:
        conn = self._get_connection()
        cursor = conn.cursor()

        cursor.execute("SELECT * FROM ma_deals WHERE deal_id = ?", (deal_id,))
        row = cursor.fetchone()
        return dict(row) if row else None

    def get_deal_financials(self, deal_id: str) -> List[Dict[str, Any]]:
        conn = self._get_connection()
        cursor = conn.cursor()

        cursor.execute("""
            SELECT * FROM deal_financials
            WHERE deal_id = ?
            ORDER BY fiscal_year DESC, fiscal_quarter DESC
        """, (deal_id,))

        return [dict(row) for row in cursor.fetchall()]

    def get_deal_multiples(self, deal_id: str) -> List[Dict[str, Any]]:
        conn = self._get_connection()
        cursor = conn.cursor()

        cursor.execute("""
            SELECT * FROM deal_multiples
            WHERE deal_id = ?
            ORDER BY calculation_date DESC
        """, (deal_id,))

        return [dict(row) for row in cursor.fetchall()]

    def get_industry_stats(self, industry: str, start_date: str, end_date: str) -> Dict[str, Any]:
        conn = self._get_connection()
        cursor = conn.cursor()

        cursor.execute("""
            SELECT
                COUNT(*) as deal_count,
                SUM(deal_value) as total_value,
                AVG(deal_value) as avg_value,
                AVG(premium_1day) as avg_premium_1day,
                AVG(premium_4week) as avg_premium_4week
            FROM ma_deals
            WHERE industry = ?
            AND announcement_date BETWEEN ? AND ?
            AND deal_value IS NOT NULL
        """, (industry, start_date, end_date))

        row = cursor.fetchone()
        return dict(row) if row else {}

    def get_comparable_transactions(self, industry: str, min_value: float, max_value: float,
                                   start_date: str, end_date: str, limit: int = 20) -> List[Dict[str, Any]]:
        conn = self._get_connection()
        cursor = conn.cursor()

        cursor.execute("""
            SELECT d.*,
                   m.ev_revenue, m.ev_ebitda, m.price_earnings
            FROM ma_deals d
            LEFT JOIN deal_multiples m ON d.deal_id = m.multiple_id
            WHERE d.industry = ?
            AND d.deal_value BETWEEN ? AND ?
            AND d.announcement_date BETWEEN ? AND ?
            AND d.deal_status = 'Completed'
            ORDER BY d.announcement_date DESC
            LIMIT ?
        """, (industry, min_value, max_value, start_date, end_date, limit))

        return [dict(row) for row in cursor.fetchall()]

    def close(self):
        if self.conn:
            self.conn.close()
            self.conn = None

def main():
    """CLI entry point - outputs JSON for Tauri integration"""
    import sys
    import json

    if len(sys.argv) < 2:
        result = {
            "success": False,
            "error": "No command specified. Usage: database_schema.py <command> [args...]"
        }
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]
    db = MADatabase()

    try:
        if command == "get_all":
            # get_all_ma_deals(filters)
            filters = json.loads(sys.argv[2]) if len(sys.argv) > 2 else None
            deals = db.get_all_deals(filters)

            result = {
                "success": True,
                "data": deals,
                "count": len(deals)
            }
            print(json.dumps(result))

        elif command == "create":
            # create_ma_deal(deal_data)
            if len(sys.argv) < 3:
                raise ValueError("Deal data required")

            deal_data = json.loads(sys.argv[2])
            deal_id = db.insert_deal(deal_data)

            result = {
                "success": True,
                "data": {"deal_id": deal_id}
            }
            print(json.dumps(result))

        elif command == "search":
            # search_ma_deals(query, search_type)
            if len(sys.argv) < 3:
                raise ValueError("Search query required")

            query = sys.argv[2]
            search_type = sys.argv[3] if len(sys.argv) > 3 else None

            deals = db.search_deals({
                'query': query,
                'search_type': search_type
            })

            result = {
                "success": True,
                "data": deals,
                "count": len(deals)
            }
            print(json.dumps(result))

        elif command == "update":
            # update_ma_deal(deal_id, updates)
            if len(sys.argv) < 4:
                raise ValueError("Deal ID and updates required")

            deal_id = sys.argv[2]
            updates = json.loads(sys.argv[3])

            db.update_deal(deal_id, updates)

            result = {
                "success": True,
                "data": {"deal_id": deal_id}
            }
            print(json.dumps(result))

        else:
            result = {
                "success": False,
                "error": f"Unknown command: {command}. Available: get_all, create, search, update"
            }
            print(json.dumps(result))
            sys.exit(1)

    except Exception as e:
        result = {
            "success": False,
            "error": str(e),
            "command": command
        }
        print(json.dumps(result))
        sys.exit(1)
    finally:
        db.close()

if __name__ == '__main__':
    main()
