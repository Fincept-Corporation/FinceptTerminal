"""워치리스트 MCP 툴 (SQLite 기반)"""

from mcp.server.fastmcp import FastMCP
from db import get_conn


def register(mcp: FastMCP) -> None:

    @mcp.tool()
    def get_watchlists() -> list[dict]:
        """등록된 워치리스트와 포함된 종목 전체 조회"""
        with get_conn() as conn:
            lists = conn.execute("SELECT id, name FROM watchlists ORDER BY name").fetchall()
            result = []
            for wl in lists:
                stocks = conn.execute(
                    "SELECT symbol, name, exchange, notes FROM watchlist_stocks WHERE watchlist_id=? ORDER BY sort_order, symbol",
                    (wl["id"],),
                ).fetchall()
                result.append({
                    "id": wl["id"],
                    "name": wl["name"],
                    "stocks": [dict(s) for s in stocks],
                })
        return result

    @mcp.tool()
    def get_watchlist_symbols() -> list[str]:
        """워치리스트에 등록된 종목 코드 전체 목록 (중복 제거)"""
        with get_conn() as conn:
            rows = conn.execute(
                "SELECT DISTINCT symbol FROM watchlist_stocks ORDER BY symbol"
            ).fetchall()
        return [r["symbol"] for r in rows]
