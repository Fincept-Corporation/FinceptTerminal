"""포트폴리오 및 보유 종목 MCP 툴 (SQLite 기반)"""

from mcp.server.fastmcp import FastMCP
from db import get_conn


def register(mcp: FastMCP) -> None:

    @mcp.tool()
    def get_holdings() -> list[dict]:
        """활성화된 포트폴리오 보유 종목 전체 조회 (종목코드, 수량, 평균매입가)"""
        with get_conn() as conn:
            rows = conn.execute(
                "SELECT id, symbol, name, shares, avg_cost, added_at FROM portfolio_holdings WHERE active=1 ORDER BY symbol"
            ).fetchall()
        return [dict(r) for r in rows]

    @mcp.tool()
    def get_portfolios() -> list[dict]:
        """등록된 투자 포트폴리오 목록 조회"""
        with get_conn() as conn:
            rows = conn.execute(
                "SELECT id, name, owner, currency, description, created_at FROM portfolios ORDER BY created_at DESC"
            ).fetchall()
        return [dict(r) for r in rows]

    @mcp.tool()
    def get_portfolio_assets(portfolio_id: str) -> list[dict]:
        """특정 포트폴리오의 보유 종목 조회

        Args:
            portfolio_id: 포트폴리오 ID (get_portfolios로 확인)
        """
        with get_conn() as conn:
            rows = conn.execute(
                "SELECT id, symbol, quantity, avg_buy_price, sector, first_purchase_date FROM portfolio_assets WHERE portfolio_id=? ORDER BY symbol",
                (portfolio_id,),
            ).fetchall()
        return [dict(r) for r in rows]

    @mcp.tool()
    def get_transactions(portfolio_id: str, limit: int = 50) -> list[dict]:
        """포트폴리오 거래 내역 조회 (BUY/SELL/DIVIDEND/SPLIT)

        Args:
            portfolio_id: 포트폴리오 ID
            limit: 최대 반환 건수 (기본 50)
        """
        with get_conn() as conn:
            rows = conn.execute(
                """SELECT id, symbol, transaction_type, quantity, price, total_amount, transaction_date, notes
                   FROM portfolio_transactions WHERE portfolio_id=?
                   ORDER BY transaction_date DESC LIMIT ?""",
                (portfolio_id, limit),
            ).fetchall()
        return [dict(r) for r in rows]

    @mcp.tool()
    def get_paper_trading_portfolios() -> list[dict]:
        """페이퍼 트레이딩(모의투자) 포트폴리오 목록 조회"""
        with get_conn() as conn:
            rows = conn.execute(
                "SELECT id, name, balance, initial_balance, currency FROM pt_portfolios ORDER BY created_at DESC"
            ).fetchall()
        return [dict(r) for r in rows]

    @mcp.tool()
    def get_paper_trading_positions(portfolio_id: str) -> list[dict]:
        """페이퍼 트레이딩 현재 보유 포지션 조회

        Args:
            portfolio_id: 페이퍼 트레이딩 포트폴리오 ID
        """
        with get_conn() as conn:
            rows = conn.execute(
                "SELECT symbol, side, quantity, avg_entry_price, unrealized_pnl FROM pt_positions WHERE portfolio_id=? AND quantity>0",
                (portfolio_id,),
            ).fetchall()
        return [dict(r) for r in rows]
