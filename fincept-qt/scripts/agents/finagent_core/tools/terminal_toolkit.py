"""
TerminalToolkit - Agno toolkit that proxies tool calls to the Fincept Terminal.

finagent_core runs as a Python subprocess of the terminal, so it reaches the
terminal's MCP tools (internal tools plus tools from the user's external MCP
servers) through the terminal's local HTTP bridge (src/mcp/TerminalMcpBridge):

  Python agent -> POST http://127.0.0.1:{port}/tool  {id, tool, args, serverId?}
              <- ToolResult JSON {success, data?, message?, error?, truncated?, ...}

The terminal injects everything this toolkit needs into the agent config:
  terminal_mcp_endpoint          bridge base URL
  terminal_mcp_token             run-scoped token (X-MCP-Token); it binds this
                                 agent's own tool policy and dies with the run
  terminal_mcp_destructive_token only when the agent opted in to state-changing
                                 tools (X-MCP-Allow-Destructive)
  terminal_tools                 the catalogue: [{name, description, inputSchema,
                                 serverId, timeoutMs}]

When an agent config names no tool categories, the catalogue is the curated
"essentials" set plus tool_list / tool_describe / tool_invoke; call_terminal_tool
(always registered) reaches any other tool the agent's policy allows.

Usage:
    toolkit = TerminalToolkit(endpoint=..., tool_definitions=[...], token=...)
    agent = Agent(tools=toolkit.get_tools(), ...)
"""

import json
import logging
import urllib.error
import urllib.request
import uuid
from typing import Any, Dict, List, Optional

logger = logging.getLogger(__name__)

INTERNAL_SERVER_ID = "fincept-terminal"

# HTTP timeout = the tool's own budget (timeoutMs from the catalogue) + slack,
# so the terminal's watchdog — not this socket — decides when a tool has taken
# too long, and the agent receives the terminal's real error.
_TIMEOUT_SLACK_SECONDS = 15.0
_DEFAULT_TOOL_TIMEOUT_SECONDS = 120.0
# Budget of the terminal's tool_invoke dispatcher (MetaTools.cpp).
_TOOL_INVOKE_TIMEOUT_SECONDS = 900.0

GENERIC_TOOL_NAME = "call_terminal_tool"


class TerminalToolkit:
    """
    Agno-compatible toolkit wrapping the terminal's MCP tools. Each catalogue
    entry becomes a function the LLM can call; calls are forwarded to the bridge.
    """

    def __init__(
        self,
        endpoint: str,
        tool_definitions: List[Dict[str, Any]],
        token: Optional[str] = None,
        destructive_token: Optional[str] = None,
        dry_run: bool = False,
        default_timeout_seconds: float = _DEFAULT_TOOL_TIMEOUT_SECONDS,
        **kwargs,
    ):
        self.endpoint = endpoint.rstrip("/")
        self.tool_definitions = [d for d in (tool_definitions or []) if isinstance(d, dict) and d.get("name")]
        self.default_timeout_seconds = float(default_timeout_seconds)
        # Run-scoped auth token issued by the terminal (TerminalMcpBridge).
        # Sent as X-MCP-Token on every request — the bridge rejects mismatches
        # with HTTP 401 and refuses tools outside this agent's policy.
        self.token = token
        # Capability token. Only present when the agent config has
        # `allow_destructive_tools=true`. When sent, the bridge permits
        # tools tagged `is_destructive=true` (e.g. order placement,
        # file deletion, settings mutation).
        self.destructive_token = destructive_token
        # Dry-run: when true, calls return a synthetic result instead of
        # crossing the HTTP bridge. Lets agents be exercised without
        # mutating real state (paper orders, file ops, settings, etc.).
        self.dry_run = dry_run

        # name -> catalogue entry (serverId / timeoutMs live here).
        self._catalog: Dict[str, Dict[str, Any]] = {d["name"]: d for d in self.tool_definitions}
        self.functions: List[Any] = []
        self._build_functions()

        logger.info(f"TerminalToolkit: {len(self.functions)} tools registered, endpoint={self.endpoint}")

    # ------------------------------------------------------------------
    # Agno Toolkit interface
    # ------------------------------------------------------------------

    def get_tools(self) -> List[Any]:
        """Returns list of Agno Function objects for use by the Agent."""
        return self.functions

    def register_with_agent(self, agent) -> None:
        """Alternative registration: append the functions to an agent's tools."""
        if hasattr(agent, "tools") and isinstance(agent.tools, list):
            agent.tools.extend(self.functions)
        elif hasattr(agent, "register"):
            for fn in self.functions:
                agent.register(fn)

    # ------------------------------------------------------------------
    # Function construction
    # ------------------------------------------------------------------

    def _build_functions(self) -> None:
        try:
            from agno.tools import Function
        except ImportError:
            logger.warning("agno.tools.Function not available — using simple callables")
            Function = None

        def add(name: str, description: str, schema: Dict[str, Any], fn) -> None:
            if Function is not None:
                try:
                    self.functions.append(
                        Function(name=name, description=description, parameters=schema, entrypoint=fn)
                    )
                    return
                except Exception as e:
                    logger.debug(f"Agno Function wrapping failed for {name}: {e}, using raw callable")
            self.functions.append(fn)

        for tool_def in self.tool_definitions:
            name: str = tool_def["name"]
            description: str = tool_def.get("description") or f"Terminal tool: {name}"
            schema: Dict[str, Any] = tool_def.get("inputSchema") or {"type": "object", "properties": {}}
            add(name, description, schema, self._make_tool_fn(name, description, schema))

        # Generic entry point: reaches tools that are not in the catalogue above
        # (found with tool_list), and pages truncated results via result_fetch.
        if GENERIC_TOOL_NAME not in self._catalog:
            add(GENERIC_TOOL_NAME, _GENERIC_DESCRIPTION, _GENERIC_SCHEMA, self._make_generic_fn())

    def _make_tool_fn(self, name: str, description: str, input_schema: Dict[str, Any]):
        """Returns a callable that, when called with kwargs, forwards to the bridge."""
        toolkit_self = self

        def tool_fn(**kwargs) -> str:
            return toolkit_self._call_tool(name, kwargs)

        tool_fn.__name__ = name
        tool_fn.__doc__ = description
        tool_fn.__annotations__ = {}
        tool_fn._input_schema = input_schema
        return tool_fn

    def _make_generic_fn(self):
        toolkit_self = self

        def call_terminal_tool(tool: str = "", arguments: Optional[Dict[str, Any]] = None,
                               server_id: str = "", **_ignored) -> str:
            return toolkit_self.call_terminal_tool(tool, arguments, server_id)

        call_terminal_tool.__doc__ = _GENERIC_DESCRIPTION
        call_terminal_tool._input_schema = _GENERIC_SCHEMA
        return call_terminal_tool

    # ------------------------------------------------------------------
    # Public helpers
    # ------------------------------------------------------------------

    def call_terminal_tool(self, tool: str, arguments: Optional[Dict[str, Any]] = None,
                           server_id: str = "") -> str:
        """Call any terminal tool by name.

        - A tool in this agent's catalogue is posted directly (its own serverId
          and timeout apply).
        - An explicit external `server_id` is posted directly to that server.
        - Otherwise the terminal's tool_invoke dispatcher is used when the
          catalogue has it (Essentials agents), else the tool is posted as an
          internal tool. The terminal enforces the agent's tool policy either way.
        """
        name = (tool or "").strip()
        if not name:
            return json.dumps({"error": "call_terminal_tool needs 'tool' (a name from tool_list)."})
        if name == GENERIC_TOOL_NAME:
            return json.dumps({"error": "call_terminal_tool cannot call itself."})
        if arguments is None:
            arguments = {}
        if isinstance(arguments, str):
            # Some models serialise nested objects; accept a JSON string.
            try:
                arguments = json.loads(arguments) if arguments.strip() else {}
            except ValueError:
                return json.dumps({"error": "'arguments' must be a JSON object."})
        if not isinstance(arguments, dict):
            return json.dumps({"error": "'arguments' must be a JSON object."})

        server_id = (server_id or "").strip()
        if name in self._catalog and (not server_id or server_id == self._catalog[name].get("serverId")):
            return self._call_tool(name, arguments)
        if server_id and server_id != INTERNAL_SERVER_ID:
            return self._post(name, arguments, server_id, self.default_timeout_seconds + _TIMEOUT_SLACK_SECONDS)
        if "tool_invoke" in self._catalog:
            return self._post("tool_invoke", {"tool": name, "arguments": arguments}, None,
                              _TOOL_INVOKE_TIMEOUT_SECONDS + _TIMEOUT_SLACK_SECONDS)
        return self._post(name, arguments, None, self.default_timeout_seconds + _TIMEOUT_SLACK_SECONDS)

    # ------------------------------------------------------------------
    # HTTP bridge call
    # ------------------------------------------------------------------

    def _call_tool(self, tool_name: str, args: Dict[str, Any]) -> str:
        """Call a catalogue tool: route by its serverId, time out by its budget."""
        entry = self._catalog.get(tool_name, {})
        server_id = entry.get("serverId") or None
        if server_id == INTERNAL_SERVER_ID:
            server_id = None  # the bridge's default
        try:
            budget_s = float(entry.get("timeoutMs") or 0) / 1000.0
        except (TypeError, ValueError):
            budget_s = 0.0
        timeout_s = (budget_s if budget_s > 0 else self.default_timeout_seconds) + _TIMEOUT_SLACK_SECONDS
        return self._post(tool_name, args, server_id, timeout_s)

    def _post(self, tool_name: str, args: Dict[str, Any], server_id: Optional[str], timeout_s: float) -> str:
        """POST {id, tool, args, serverId?} to the bridge; return a string for the LLM."""
        payload: Dict[str, Any] = {"id": str(uuid.uuid4()), "tool": tool_name, "args": args}
        # External tools MUST carry their server id, otherwise the bridge routes
        # the call to the internal registry and it fails as an unknown tool.
        if server_id:
            payload["serverId"] = server_id

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
            headers = {"Content-Type": "application/json", "Content-Length": str(len(data))}
            if self.token:
                headers["X-MCP-Token"] = self.token
            if self.destructive_token:
                headers["X-MCP-Allow-Destructive"] = self.destructive_token
            req = urllib.request.Request(f"{self.endpoint}/tool", data=data, headers=headers, method="POST")
            with urllib.request.urlopen(req, timeout=timeout_s) as resp:
                response_body = resp.read().decode("utf-8")
            result = json.loads(response_body)
        except urllib.error.HTTPError as e:
            # 401 (run ended / bad token), 403, 413 … — surface the bridge's reason.
            detail = ""
            try:
                detail = json.loads(e.read().decode("utf-8")).get("error", "")
            except Exception:
                pass
            logger.error(f"Terminal tool '{tool_name}' HTTP {e.code}: {detail or e.reason}")
            return json.dumps({"error": f"Terminal bridge refused the call (HTTP {e.code}): {detail or e.reason}"})
        except urllib.error.URLError as e:
            logger.error(f"Terminal tool '{tool_name}' bridge error: {e}")
            return json.dumps({"error": f"Bridge unreachable: {e}"})
        except TimeoutError:
            logger.error(f"Terminal tool '{tool_name}' timed out after {timeout_s:.0f}s")
            return json.dumps({"error": f"Tool '{tool_name}' did not answer within {timeout_s:.0f}s"})
        except json.JSONDecodeError as e:
            logger.error(f"Terminal tool '{tool_name}' JSON decode error: {e}")
            return json.dumps({"error": "Invalid JSON response from bridge"})
        except Exception as e:
            logger.error(f"Terminal tool '{tool_name}' unexpected error: {e}")
            return json.dumps({"error": str(e)})

        if result.get("success") is False or result.get("error"):
            error_msg = result.get("error", "Tool execution failed")
            logger.warning(f"Terminal tool '{tool_name}' returned error: {error_msg}")
            return json.dumps({"error": error_msg})

        data_val = result.get("data")
        if data_val is None:
            data_val = result.get("message", "Tool executed successfully")

        # The bridge shapes oversized results and says so in place; keep those
        # markers — an agent that read a trimmed array as complete would reason
        # off a truncated dataset.
        if result.get("truncated"):
            return json.dumps({
                "data": data_val,
                "truncated": True,
                "result_id": result.get("result_id"),
                "full_bytes": result.get("full_bytes"),
                "note": (result.get("note") or "")
                + " Use " + GENERIC_TOOL_NAME + "(tool='result_fetch', arguments={'result_id': ..., "
                  "'offset': ..., 'limit': ...}) to page it.",
            })

        return json.dumps(data_val) if not isinstance(data_val, str) else data_val


_GENERIC_DESCRIPTION = (
    "Call any Fincept Terminal tool by its exact name, including tools that are not listed "
    "among your functions. Find tools with tool_list and read their parameters with "
    "tool_describe first. Pass server_id only for a tool on an external MCP server (as "
    "tool_describe reports it). Also pages a truncated result: tool='result_fetch'."
)

_GENERIC_SCHEMA: Dict[str, Any] = {
    "type": "object",
    "properties": {
        "tool": {"type": "string", "description": "Exact tool name (from tool_list / tool_describe)"},
        "arguments": {"type": "object", "description": "Arguments object matching the tool's input schema"},
        "server_id": {"type": "string", "description": "External MCP server id; omit for terminal tools"},
    },
    "required": ["tool"],
}
