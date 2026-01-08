#!/usr/bin/env python3
"""
Core Agent CLI - Simple interface for Rust/Tauri integration

Follows yfinance pattern - all I/O via JSON.
"""

import sys
import json
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent))

from finagent_core import CoreAgent


def main(args=None):
    """
    CLI entry point for CoreAgent

    Usage:
        python core_agent_cli.py <query_json> <config_json> <api_keys_json>

    Example:
        python core_agent_cli.py \
            '{"query": "What is AI?"}' \
            '{"model": {"provider": "openai", "model_id": "gpt-4-turbo"}, "instructions": "Helpful assistant"}' \
            '{"OPENAI_API_KEY": "sk-..."}'
    """
    if args is None:
        args = sys.argv[1:]

    try:
        # Parse arguments
        if len(args) < 3:
            return json.dumps({
                "success": False,
                "error": "Usage: core_agent_cli.py <query_json> <config_json> <api_keys_json>"
            })

        query_json = json.loads(args[0])
        config = json.loads(args[1])
        api_keys = json.loads(args[2])

        # Optional session_id and stream
        session_id = query_json.get("session_id")
        stream = query_json.get("stream", False)
        query = query_json.get("query")

        if not query:
            return json.dumps({
                "success": False,
                "error": "Missing 'query' in query_json"
            })

        # Create and run agent
        agent = CoreAgent(api_keys=api_keys)
        response = agent.run(
            query=query,
            config=config,
            session_id=session_id,
            stream=stream
        )

        # Extract content
        content = agent.get_response_content(response)

        # Return result
        result = {
            "success": True,
            "response": content,
            "config": config,
            "session_id": session_id
        }

        return json.dumps(result)

    except json.JSONDecodeError as e:
        return json.dumps({
            "success": False,
            "error": f"Invalid JSON: {str(e)}"
        })
    except Exception as e:
        return json.dumps({
            "success": False,
            "error": str(e)
        })


if __name__ == "__main__":
    result = main()
    print(result)
