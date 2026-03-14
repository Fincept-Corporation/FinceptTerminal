"""
AKShare Symbols Database Builder
Downloads all stock symbols from 9 exchanges/markets and stores in SQLite for fast searching
"""

import sqlite3
import akshare as ak
import pandas as pd
from datetime import datetime
import os
import sys

# Store in AppData like other terminal files
APPDATA = os.environ.get('APPDATA', os.path.expanduser('~/.config'))
DB_DIR = os.path.join(APPDATA, 'fincept-terminal')
os.makedirs(DB_DIR, exist_ok=True)
DB_PATH = os.path.join(DB_DIR, 'akshare_symbols.db')

def create_database():
    """Create SQLite database with symbols table"""
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()

    # Drop existing table if exists
    cursor.execute('DROP TABLE IF EXISTS symbols')

    # Create symbols table
    cursor.execute('''
        CREATE TABLE symbols (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            exchange TEXT NOT NULL,
            symbol TEXT NOT NULL,
            name TEXT,
            name_en TEXT,
            latest_price REAL,
            change REAL,
            change_pct REAL,
            volume REAL,
            turnover REAL,
            market_cap REAL,
            last_updated TIMESTAMP,
            UNIQUE(exchange, symbol)
        )
    ''')

    # Create indexes for fast searching
    cursor.execute('CREATE INDEX idx_exchange ON symbols(exchange)')
    cursor.execute('CREATE INDEX idx_symbol ON symbols(symbol)')
    cursor.execute('CREATE INDEX idx_name ON symbols(name)')
    cursor.execute('CREATE INDEX idx_search ON symbols(exchange, symbol, name)')

    conn.commit()
    conn.close()
    print(f"[OK] Database created: {DB_PATH}")

def fetch_and_insert(exchange_name, fetch_function, column_mapping):
    """Fetch data from AKShare and insert into database"""
    print(f"\n> Fetching {exchange_name}...")

    try:
        df = fetch_function()

        if df is None or df.empty:
            print(f"  [FAIL] No data returned for {exchange_name}")
            return 0

        print(f"  Found {len(df)} symbols")

        # Prepare data for insertion
        records = []
        timestamp = datetime.now().isoformat()

        for _, row in df.iterrows():
            record = {
                'exchange': exchange_name,
                'symbol': str(row.get(column_mapping.get('symbol', '代码'), '')),
                'name': str(row.get(column_mapping.get('name', '名称'), '')),
                'name_en': str(row.get(column_mapping.get('name_en', '英文名称'), '')) if 'name_en' in column_mapping else None,
                'latest_price': float(row.get(column_mapping.get('price', '最新价'), 0)) if pd.notna(row.get(column_mapping.get('price', '最新价'))) else None,
                'change': float(row.get(column_mapping.get('change', '涨跌额'), 0)) if pd.notna(row.get(column_mapping.get('change', '涨跌额'))) else None,
                'change_pct': float(row.get(column_mapping.get('change_pct', '涨跌幅'), 0)) if pd.notna(row.get(column_mapping.get('change_pct', '涨跌幅'))) else None,
                'volume': float(row.get(column_mapping.get('volume', '成交量'), 0)) if pd.notna(row.get(column_mapping.get('volume', '成交量'))) else None,
                'turnover': float(row.get(column_mapping.get('turnover', '成交额'), 0)) if pd.notna(row.get(column_mapping.get('turnover', '成交额'))) else None,
                'market_cap': float(row.get(column_mapping.get('market_cap', '总市值'), 0)) if pd.notna(row.get(column_mapping.get('market_cap', '总市值'))) else None,
                'last_updated': timestamp
            }
            records.append(record)

        # Insert into database
        conn = sqlite3.connect(DB_PATH)
        cursor = conn.cursor()

        for record in records:
            cursor.execute('''
                INSERT OR REPLACE INTO symbols
                (exchange, symbol, name, name_en, latest_price, change, change_pct, volume, turnover, market_cap, last_updated)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ''', (
                record['exchange'], record['symbol'], record['name'], record['name_en'],
                record['latest_price'], record['change'], record['change_pct'],
                record['volume'], record['turnover'], record['market_cap'], record['last_updated']
            ))

        conn.commit()
        conn.close()

        print(f"  [OK] Inserted {len(records)} symbols")
        return len(records)

    except Exception as e:
        print(f"  [ERROR] {str(e)}")
        return 0

def build_database():
    """Main function to build the complete symbols database"""
    print("=" * 60)
    print("AKShare Symbols Database Builder")
    print("=" * 60)

    create_database()

    total_symbols = 0

    # Exchange configurations
    exchanges = [
        {
            'name': 'CN_A_SHANGHAI',
            'function': ak.stock_sh_a_spot_em,
            'mapping': {'symbol': '代码', 'name': '名称', 'price': '最新价', 'change': '涨跌额',
                       'change_pct': '涨跌幅', 'volume': '成交量', 'turnover': '成交额', 'market_cap': '总市值'}
        },
        {
            'name': 'CN_A_SHENZHEN',
            'function': ak.stock_sz_a_spot_em,
            'mapping': {'symbol': '代码', 'name': '名称', 'price': '最新价', 'change': '涨跌额',
                       'change_pct': '涨跌幅', 'volume': '成交量', 'turnover': '成交额', 'market_cap': '总市值'}
        },
        {
            'name': 'CN_BEIJING',
            'function': ak.stock_bj_a_spot_em,
            'mapping': {'symbol': '代码', 'name': '名称', 'price': '最新价', 'change': '涨跌额',
                       'change_pct': '涨跌幅', 'volume': '成交量', 'turnover': '成交额', 'market_cap': '总市值'}
        },
        {
            'name': 'CN_CHINEXT',
            'function': ak.stock_cy_a_spot_em,
            'mapping': {'symbol': '代码', 'name': '名称', 'price': '最新价', 'change': '涨跌额',
                       'change_pct': '涨跌幅', 'volume': '成交量', 'turnover': '成交额', 'market_cap': '总市值'}
        },
        {
            'name': 'CN_STAR',
            'function': ak.stock_kc_a_spot_em,
            'mapping': {'symbol': '代码', 'name': '名称', 'price': '最新价', 'change': '涨跌额',
                       'change_pct': '涨跌幅', 'volume': '成交量', 'turnover': '成交额', 'market_cap': '总市值'}
        },
        {
            'name': 'CN_B_SHARES',
            'function': ak.stock_zh_b_spot_em,
            'mapping': {'symbol': '代码', 'name': '名称', 'price': '最新价', 'change': '涨跌额',
                       'change_pct': '涨跌幅', 'volume': '成交量', 'turnover': '成交额'}
        },
        {
            'name': 'HK',
            'function': ak.stock_hk_spot_em,
            'mapping': {'symbol': '代码', 'name': '名称', 'name_en': '英文名称', 'price': '最新价',
                       'change': '涨跌额', 'change_pct': '涨跌幅', 'volume': '成交量', 'turnover': '成交额'}
        },
        {
            'name': 'US',
            'function': ak.stock_us_spot_em,
            'mapping': {'symbol': '代码', 'name': '名称', 'price': '最新价', 'change': '涨跌额',
                       'change_pct': '涨跌幅', 'volume': '成交量', 'turnover': '成交额', 'market_cap': '总市值'}
        },
        {
            'name': 'US_PINK',
            'function': ak.stock_us_pink_spot_em,
            'mapping': {'symbol': '代码', 'name': '名称', 'price': '最新价', 'change': '涨跌额',
                       'change_pct': '涨跌幅', 'volume': '成交量', 'turnover': '成交额'}
        },
    ]

    # Fetch and insert data for each exchange
    for exchange_config in exchanges:
        count = fetch_and_insert(
            exchange_config['name'],
            exchange_config['function'],
            exchange_config['mapping']
        )
        total_symbols += count

    # Print summary
    print("\n" + "=" * 60)
    print("Database Build Complete")
    print("=" * 60)
    print(f"Total symbols: {total_symbols}")
    print(f"Database location: {DB_PATH}")
    print(f"Database size: {os.path.getsize(DB_PATH) / 1024 / 1024:.2f} MB")

    # Print exchange breakdown
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute('SELECT exchange, COUNT(*) FROM symbols GROUP BY exchange ORDER BY exchange')
    print("\nBreakdown by exchange:")
    for exchange, count in cursor.fetchall():
        print(f"  {exchange}: {count:,} symbols")
    conn.close()

if __name__ == '__main__':
    try:
        build_database()
        print("\n[OK] Success!")
    except KeyboardInterrupt:
        print("\n\n[STOP] Interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n[ERROR] Fatal error: {str(e)}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
