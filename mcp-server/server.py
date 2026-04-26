"""
Fincept Terminal MCP 서버
Claude Code / Codex 등 AI 도구에서 주식분석, 포트폴리오, 뉴스 데이터를 조회한다.

실행:
    /Users/shin/01_Myworkspace/FinceptTerminal/.venv/bin/python server.py
"""

import sys
import os

# 현재 디렉토리를 경로에 추가 (db.py, tools/ 임포트를 위해)
sys.path.insert(0, os.path.dirname(__file__))

from mcp.server.fastmcp import FastMCP

mcp = FastMCP("fincept-terminal")

# 툴 등록
from tools import portfolio, news, watchlist, markets

portfolio.register(mcp)
news.register(mcp)
watchlist.register(mcp)
markets.register(mcp)

if __name__ == "__main__":
    mcp.run()
