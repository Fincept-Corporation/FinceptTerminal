"""
TerminalToolkit - Agno Toolkit that proxies calls to Fincept Terminal internal MCP tools.

When finagent_core runs as a Python subprocess, it cannot call C++ IPC directly.
This toolkit bridges that gap via the local HTTP bridge (mcp_bridge.rs):

  Python agent → POST http://127.0.0.1:{port}/tool → host bridge
    → C++ MCP service call
    → register_mcp_tool_result → host bridge → HTTP response → Python

Usage:
    toolkit = TerminalToolkit(
        endpoint="http://127.0.0.1:12345",
        tool_definitions=[
            {"name": "get_quote", "description": "...", "inputSchema": {...}},
            ...
        ]
    )
    agent = Agent(tools=[toolkit], ...)
"""

import json
import uuid
import logging
import urllib.request
import urllib.error
from typing import Any, Dict, List, Optional

logger = logging.getLogger(__name__)


class TerminalToolkit:
    """
    Agno-compatible toolkit that wraps Fincept Terminal's internal MCP tools.

    Each tool definition becomes a Python function registered with Agno so the LLM
    can call it. Calls are forwarded to the local HTTP bridge which routes them to
    the host TerminalMCPProvider.
    """

    def __init__(
        self,
        endpoint: str,
        tool_definitions: List[Dict[str, Any]],
        timeout_seconds: int = 85,
        token: Optional[str] = None,
        destructive_token: Optional[str] = None,
        dry_run: bool = False,
        **kwargs,
    ):
        self.endpoint = endpoint.rstrip("/")
        self.tool_definitions = tool_definitions
        self.timeout_seconds = timeout_seconds
        # Per-process auth token issued by the Qt host (TerminalMcpBridge).
        # Sent as X-MCP-Token on every request — bridge rejects mismatches
        # with HTTP 401.
        self.token = token
        # Capability token. Only present when the agent config has
        # `allow_destructive_tools=true`. When sent, the bridge permits
        # tools tagged `is_destructive=true` (e.g. order placement,
        # file deletion, settings mutation).
        self.destructive_token = destructive_token
        # Dry-run: when true, _call_tool returns a synthetic result instead
        # of crossing the HTTP bridge. Lets agents be exercised without
        # mutating real state (paper orders, file ops, settings, etc.).
        self.dry_run = dry_run
        self.functions: List[Any] = []

        # Build Agno-compatible function wrappers
        self._build_functions()

        logger.info(
            f"TerminalToolkit: {len(self.functions)} tools registered, endpoint={self.endpoint}"
        )

    # ------------------------------------------------------------------
    # Agno Toolkit interface
    # ------------------------------------------------------------------

    def get_tools(self) -> List[Any]:
        """Returns list of Agno Function objects for use by the Agent."""
        return self.functions

    # ------------------------------------------------------------------
    # Internal: build one Python function per tool definition
    # ------------------------------------------------------------------

    def _build_functions(self) -> None:
        """
        Dynamically create a Python callable for each tool definition and wrap it
        as an Agno Function so the agent can discover and call it.
        """
        try:
            from agno.tools import Function
        except ImportError:
            logger.warning("agno.tools.Function not available — using simple callables")
            Function = None

        for tool_def in self.tool_definitions:
            name: str = tool_def.get("name", "")
            description: str = tool_def.get("description", f"Terminal tool: {name}")
            input_schema: Dict = tool_def.get("inputSchema", {"type": "object", "properties": {}})

            if not name:
                continue

            # Create a closure that captures `name`
            fn = self._make_tool_fn(name, description, input_schema)

            if Function is not None:
                try:
                    # Register as Agno Function with proper schema
                    agno_fn = Function(
                        name=name,
                        description=description,
                        parameters=input_schema,
                        entrypoint=fn,
                    )
                    self.functions.append(agno_fn)
                    continue
                except Exception as e:
                    logger.debug(f"Agno Function wrapping failed for {name}: {e}, using raw callable")

            # Fallback: raw callable (Agno can sometimes infer tools from callables)
            self.functions.append(fn)

    def _make_tool_fn(
        self, name: str, description: str, input_schema: Dict
    ):
        """Returns a callable that, when called with kwargs, forwards to the HTTP bridge."""
        toolkit_self = self

        def tool_fn(**kwargs) -> str:
            return toolkit_self._call_tool(name, kwargs)

        # Set function metadata so Agno can use it
        tool_fn.__name__ = name
        tool_fn.__doc__ = description

        # Attach schema for Agno introspection
        tool_fn.__annotations__ = {}
        tool_fn._input_schema = input_schema

        return tool_fn

    # ------------------------------------------------------------------
    # HTTP bridge call
    # ------------------------------------------------------------------

    def _call_tool(self, tool_name: str, args: Dict[str, Any]) -> str:
        """
        POST { id, tool, args } to the host HTTP bridge.
        Returns the tool result as a JSON string for the LLM.
        """
        call_id = str(uuid.uuid4())
        payload = {
            "id": call_id,
            "tool": tool_name,
            "args": args,
        }

        # Dry-run short-circuit — never crosses the bridge. The synthetic
        # response tells the LLM the call would have succeeded so the agent
        # loop can continue past the tool step without touching real state.
        if self.dry_run:
            logger.info(f"TerminalToolkit[dry-run] {tool_name}({args})")
            return json.dumps({
                "dry_run": True,
                "tool": tool_name,
                "args": args,
                "message": f"[dry-run] {tool_name} would have been called with the supplied arguments",
            })

        try:
            data = json.dumps(payload).encode("utf-8")
            headers = {
                "Content-Type": "application/json",
                "Content-Length": str(len(data)),
            }
            if self.token:
                headers["X-MCP-Token"] = self.token
            if self.destructive_token:
                headers["X-MCP-Allow-Destructive"] = self.destructive_token
            req = urllib.request.Request(
                f"{self.endpoint}/tool",
                data=data,
                headers=headers,
                method="POST",
            )

            with urllib.request.urlopen(req, timeout=self.timeout_seconds) as resp:
                response_body = resp.read().decode("utf-8")

            result = json.loads(response_body)

            if result.get("success") is False or result.get("error"):
                error_msg = result.get("error", "Tool execution failed")
                logger.warning(f"Terminal tool '{tool_name}' returned error: {error_msg}")
                return json.dumps({"error": error_msg})

            data_val = result.get("data")
            if data_val is None:
                data_val = result.get("message", "Tool executed successfully")

            return json.dumps(data_val) if not isinstance(data_val, str) else data_val

        except urllib.error.URLError as e:
            logger.error(f"Terminal tool '{tool_name}' HTTP error: {e}")
            return json.dumps({"error": f"Bridge unreachable: {e}"})
        except json.JSONDecodeError as e:
            logger.error(f"Terminal tool '{tool_name}' JSON decode error: {e}")
            return json.dumps({"error": "Invalid JSON response from bridge"})
        except Exception as e:
            logger.error(f"Terminal tool '{tool_name}' unexpected error: {e}")
            return json.dumps({"error": str(e)})

    # ------------------------------------------------------------------
    # Agno Toolkit interface (alternative registration style)
    # ------------------------------------------------------------------

    def register_with_agent(self, agent) -> None:
        """
        Alternative: directly register all tool functions into an Agno agent's tool list.
        Called if the standard get_tools() approach doesn't work.
        """
        if hasattr(agent, "tools") and isinstance(agent.tools, list):
            agent.tools.extend(self.functions)
        elif hasattr(agent, "register"):
            for fn in self.functions:
                agent.register(fn)
