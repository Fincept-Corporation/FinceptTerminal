"""
Edgar MCP Wrapper
==================

Wrapper script to execute Edgar MCP commands.
This adds the scripts directory to Python path and calls the main module.

Usage:
    python edgar_mcp.py <command> [args...]
"""

import sys
import os

# Add scripts directory to path
scripts_dir = os.path.dirname(os.path.abspath(__file__))
if scripts_dir not in sys.path:
    sys.path.insert(0, scripts_dir)

# Import and run main
from mcp.edgar.main import main

if __name__ == "__main__":
    main()
