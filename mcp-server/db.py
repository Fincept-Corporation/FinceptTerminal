"""
FinceptTerminal SQLite 연결 헬퍼
앱 데이터베이스를 읽기 전용으로 접근한다.
"""

import sqlite3
import os
from pathlib import Path

DB_PATH = Path.home() / "Library" / "Application Support" / "com.fincept.terminal" / "data" / "fincept.db"


def get_conn() -> sqlite3.Connection:
    if not DB_PATH.exists():
        raise FileNotFoundError(f"FinceptTerminal DB를 찾을 수 없음: {DB_PATH}\n앱을 먼저 실행하세요.")
    conn = sqlite3.connect(f"file:{DB_PATH}?mode=ro", uri=True)
    conn.row_factory = sqlite3.Row
    return conn
