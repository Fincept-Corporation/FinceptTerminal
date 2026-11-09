"""
Fincept MCP Scripts
===================

Fincept's MCP-related scripts: the SEC EDGAR command tree (edgar/) and the
stdio bridge external MCP clients use to reach the terminal's MCP server
(fincept_mcp_stdio.py).

Named `fincept_mcp`, never `mcp`: the scripts folder is on PYTHONPATH for
every script the terminal runs, so a top-level `mcp` package here would hide
the `mcp` SDK (agno's MCP tools, FastMCP servers) from all of them.
"""

__version__ = "1.0.0"
