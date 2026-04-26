"""금융 뉴스 MCP 툴 (SQLite 기반)"""

from mcp.server.fastmcp import FastMCP
from db import get_conn


def register(mcp: FastMCP) -> None:

    @mcp.tool()
    def get_news(category: str = "", limit: int = 20) -> list[dict]:
        """최신 금융 뉴스 헤드라인 조회

        Args:
            category: 필터 카테고리. MARKETS|EARNINGS|ECONOMIC|CRYPTO|GEOPOLITICS|ENERGY|DEFENSE 중 하나. 빈 문자열이면 전체.
            limit: 최대 반환 건수 (기본 20, 최대 100)
        """
        limit = min(limit, 100)
        with get_conn() as conn:
            if category:
                rows = conn.execute(
                    "SELECT headline, summary, source, category, sentiment, tickers, sort_ts FROM news_articles WHERE category=? ORDER BY sort_ts DESC LIMIT ?",
                    (category.upper(), limit),
                ).fetchall()
            else:
                rows = conn.execute(
                    "SELECT headline, summary, source, category, sentiment, tickers, sort_ts FROM news_articles ORDER BY sort_ts DESC LIMIT ?",
                    (limit,),
                ).fetchall()
        return [dict(r) for r in rows]

    @mcp.tool()
    def search_news(query: str, limit: int = 20) -> list[dict]:
        """키워드 또는 티커로 뉴스 검색

        Args:
            query: 검색어 (회사명, 키워드, 티커 심볼)
            limit: 최대 반환 건수 (기본 20)
        """
        like = f"%{query}%"
        with get_conn() as conn:
            rows = conn.execute(
                """SELECT headline, summary, source, category, sentiment, tickers, sort_ts
                   FROM news_articles
                   WHERE headline LIKE ? OR summary LIKE ? OR tickers LIKE ?
                   ORDER BY sort_ts DESC LIMIT ?""",
                (like, like, like, limit),
            ).fetchall()
        return [dict(r) for r in rows]

    @mcp.tool()
    def get_news_by_ticker(ticker: str, limit: int = 20) -> list[dict]:
        """특정 종목(티커)의 관련 뉴스 조회

        Args:
            ticker: 종목 코드 (예: AAPL, TSLA, BTC)
            limit: 최대 반환 건수 (기본 20)
        """
        like = f"%{ticker.upper()}%"
        with get_conn() as conn:
            rows = conn.execute(
                """SELECT headline, summary, source, category, sentiment, tickers, sort_ts
                   FROM news_articles WHERE tickers LIKE ?
                   ORDER BY sort_ts DESC LIMIT ?""",
                (like, limit),
            ).fetchall()
        return [dict(r) for r in rows]

    @mcp.tool()
    def get_top_news(limit: int = 10) -> list[dict]:
        """FLASH/URGENT/BREAKING 등 긴급 뉴스만 조회

        Args:
            limit: 최대 반환 건수 (기본 10)
        """
        with get_conn() as conn:
            rows = conn.execute(
                """SELECT headline, summary, source, category, sentiment, priority, sort_ts
                   FROM news_articles WHERE priority IN ('FLASH','URGENT','BREAKING')
                   ORDER BY sort_ts DESC LIMIT ?""",
                (limit,),
            ).fetchall()
        return [dict(r) for r in rows]

    @mcp.tool()
    def get_news_sentiment_summary() -> dict:
        """카테고리별 뉴스 수량 및 감성(긍정/부정/중립) 통계 요약"""
        with get_conn() as conn:
            rows = conn.execute(
                "SELECT category, sentiment, COUNT(*) as cnt FROM news_articles GROUP BY category, sentiment"
            ).fetchall()
        summary: dict = {}
        for r in rows:
            cat = r["category"] or "UNKNOWN"
            sent = r["sentiment"] or "NEUTRAL"
            summary.setdefault(cat, {})[sent] = r["cnt"]
        return summary
