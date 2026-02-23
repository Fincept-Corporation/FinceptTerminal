"""
Fincept LLM Model - Custom adapter for Fincept's built-in LLM endpoint.

The /research/llm endpoint wraps GLM (via Anthropic API format at z.ai):
  - Accepts: prompt (str), tools (list), tool_choice (dict)
  - Returns: {"data": {"response": str, "tool_calls": [...], "model": ...}}
  - tool_calls format: [{"id", "type": "function", "function": {"name", "arguments": json_str}}]

This adapter:
  1. Converts Agno Function objects → Anthropic-style tool schemas
  2. Sends them with the prompt to /research/llm
  3. Parses tool_use blocks from the response
  4. Executes the tools via Function.entrypoint
  5. Loops with tool results until the model produces a final text response
"""

import httpx
import json
import logging
import threading
import time
from dataclasses import dataclass
from typing import Any, Optional, Dict, List, Iterator, AsyncIterator

from agno.models.base import Model
from agno.models.message import Message
from agno.models.response import ModelResponse

logger = logging.getLogger(__name__)

# ── Cross-process rate limiter: max 1 request/second to Fincept endpoint ──────
# Uses a lock file so multiple Python subprocesses (AI panel + Agent panel)
# share the same rate limit rather than each having their own in-memory counter.
import os
import tempfile

_RATE_LOCK_FILE = os.path.join(tempfile.gettempdir(), "fincept_llm_rate.lock")
_RATE_TS_FILE   = os.path.join(tempfile.gettempdir(), "fincept_llm_rate.ts")
_fincept_proc_lock = threading.Lock()  # within-process serialisation

def _rate_limit_fincept() -> None:
    """
    Block until at least 1.2 seconds have passed since the last call
    across ALL Python processes on this machine.
    Uses an exclusive file lock + a shared timestamp file.
    """
    with _fincept_proc_lock:
        # Acquire the cross-process file lock
        while True:
            try:
                fd = os.open(_RATE_LOCK_FILE, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
                os.close(fd)
                break  # we hold the lock
            except FileExistsError:
                time.sleep(0.05)

        try:
            # Read last call timestamp
            last_ts = 0.0
            try:
                with open(_RATE_TS_FILE, "r") as f:
                    last_ts = float(f.read().strip())
            except Exception:
                pass

            # Wait until 1.2 s have elapsed since last call
            now = time.time()
            wait = 1.2 - (now - last_ts)
            if wait > 0:
                time.sleep(wait)

            # Write current timestamp
            with open(_RATE_TS_FILE, "w") as f:
                f.write(str(time.time()))
        finally:
            # Always release the file lock
            try:
                os.remove(_RATE_LOCK_FILE)
            except Exception:
                pass

FINCEPT_DEFAULT_URL = "https://api.fincept.in/research/llm"


# ── Anthropic tool schema builder ─────────────────────────────────────────────

def _function_to_anthropic_tool(fn: Any) -> Dict[str, Any]:
    """
    Convert an Agno Function object to Anthropic tool schema format.

    Anthropic format:
    {
        "name": "tool_name",
        "description": "...",
        "input_schema": {
            "type": "object",
            "properties": {"param": {"type": "string", "description": "..."}},
            "required": ["param"]
        }
    }
    """
    name = fn.name if hasattr(fn, "name") else str(fn)
    description = ""
    if hasattr(fn, "description") and fn.description:
        description = str(fn.description).strip()

    # Try to get parameter schema from Agno Function
    input_schema: Dict[str, Any] = {"type": "object", "properties": {}}

    if hasattr(fn, "parameters") and fn.parameters:
        params = fn.parameters
        if isinstance(params, dict):
            # Agno stores parameters as JSON schema already
            input_schema = params
        elif hasattr(params, "model_json_schema"):
            input_schema = params.model_json_schema()
    elif hasattr(fn, "entrypoint") and callable(fn.entrypoint):
        # Derive from function signature
        import inspect
        try:
            sig = inspect.signature(fn.entrypoint)
            props: Dict[str, Any] = {}
            required: List[str] = []
            for pname, param in sig.parameters.items():
                if pname in ("self", "agent", "team", "run_context"):
                    continue
                ptype = "string"
                if param.annotation != inspect.Parameter.empty:
                    ann = param.annotation
                    if ann in (int,):
                        ptype = "integer"
                    elif ann in (float,):
                        ptype = "number"
                    elif ann in (bool,):
                        ptype = "boolean"
                props[pname] = {"type": ptype}
                if param.default is inspect.Parameter.empty:
                    required.append(pname)
            input_schema = {"type": "object", "properties": props}
            if required:
                input_schema["required"] = required
        except Exception:
            pass

    return {
        "name": name,
        "description": description or f"Tool: {name}",
        "input_schema": input_schema,
    }


# ─────────────────────────────────────────────────────────────────────────────

@dataclass
class FinceptChat(Model):
    """
    Agno-compatible model for Fincept's /research/llm endpoint.

    Supports native function calling — the endpoint wraps GLM via
    Anthropic's API format, so tool schemas and tool_use responses
    work identically to the Anthropic SDK.

    Tool-calling loop:
      1. Send prompt + Anthropic-format tool schemas
      2. If response contains tool_calls → execute them via Function.entrypoint
      3. Append tool results and call again
      4. Repeat until plain text response (no tool_calls)
    """

    id: str = "fincept-llm"
    name: str = "FinceptChat"
    provider: str = "Fincept"

    api_key: Optional[str] = None
    base_url: str = FINCEPT_DEFAULT_URL
    temperature: Optional[float] = None
    max_tokens: Optional[int] = None

    # Max tool call rounds before forcing a final answer
    # Keep low for Fincept endpoint — each round = 1 API call, gateway times out after ~60s
    max_tool_rounds: int = 5

    # ── HTTP request ──────────────────────────────────────────────────────────

    def _try_url(
        self,
        url: str,
        payload: Dict[str, Any],
        headers: Dict[str, str],
    ) -> Dict[str, Any]:
        """
        Attempt a single POST to `url`. Retries up to 3 times on 429/502/504/timeout.
        Raises RuntimeError on persistent failure so the caller can try a fallback URL.
        """
        last_error: Optional[RuntimeError] = None
        for attempt in range(3):
            try:
                with httpx.Client(timeout=250.0) as client:
                    resp = client.post(url, json=payload, headers=headers)

                    if resp.status_code == 429:
                        wait = 3.0 * (attempt + 1)  # 3s, 6s, 9s
                        logger.warning(f"FinceptChat: 429 from {url} attempt {attempt + 1}, waiting {wait}s")
                        time.sleep(wait)
                        last_error = RuntimeError(f"Fincept API error (429): rate limited")
                        continue

                    if resp.status_code in (502, 504):
                        wait = 2.0 * (attempt + 1)  # 2s, 4s, 6s
                        logger.warning(f"FinceptChat: {resp.status_code} from {url} attempt {attempt + 1}, waiting {wait}s")
                        time.sleep(wait)
                        last_error = RuntimeError(f"Fincept API error ({resp.status_code}): gateway error")
                        continue

                    if resp.status_code != 200:
                        error_detail = ""
                        try:
                            err_data = resp.json()
                            error_detail = err_data.get("detail", err_data.get("message", ""))
                        except Exception:
                            error_detail = resp.text[:200]
                        raise RuntimeError(f"Fincept API error ({resp.status_code}): {error_detail}")

                    outer = resp.json()
                    if isinstance(outer, dict) and "data" in outer:
                        return outer["data"]
                    return outer

            except httpx.TimeoutException:
                last_error = RuntimeError(f"Fincept API request timed out (250s) for {url}")
                logger.warning(f"FinceptChat: timeout from {url} attempt {attempt + 1}")
                continue
            except httpx.ConnectError as e:
                last_error = RuntimeError(f"Cannot connect to {url}: {e}")
                logger.warning(f"FinceptChat: connect error for {url}: {e}")
                break  # Connection refused — no point retrying same URL
            except RuntimeError:
                raise
            except Exception as e:
                raise RuntimeError(f"Fincept API call failed: {e}")

        raise last_error or RuntimeError(f"Fincept API failed after 3 attempts on {url}")

    def _call_endpoint(
        self,
        prompt: str,
        tools: Optional[List[Dict]] = None,
        tool_choice: Optional[Dict] = None,
        tool_results_context: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        Call /research/llm and return the parsed response dict.

        Tries self.base_url first. If that URL is different from the canonical
        FINCEPT_DEFAULT_URL and it fails with a gateway error, automatically
        falls back to the canonical URL so a misconfigured/expired custom
        endpoint never permanently blocks the agent.

        Returns the full data dict:
          {"response": str, "tool_calls": [...], "model": str, ...}
        """
        _rate_limit_fincept()

        headers = {"Content-Type": "application/json"}
        if self.api_key:
            headers["X-API-Key"] = self.api_key

        # Build the full prompt — include tool results from prior rounds
        full_prompt = prompt
        if tool_results_context:
            full_prompt = f"{prompt}\n\n{tool_results_context}"

        payload: Dict[str, Any] = {"prompt": full_prompt}
        if self.temperature is not None:
            payload["temperature"] = self.temperature
        if self.max_tokens is not None:
            payload["max_tokens"] = self.max_tokens
        if tools:
            payload["tools"] = tools
        if tool_choice:
            payload["tool_choice"] = tool_choice

        # Try the configured URL first
        try:
            return self._try_url(self.base_url, payload, headers)
        except RuntimeError as primary_err:
            # If we're already using the default URL, re-raise immediately
            if self.base_url == FINCEPT_DEFAULT_URL:
                raise

            # Custom/tunnel URL failed — fall back to the real Fincept API
            logger.warning(
                f"FinceptChat: custom base_url '{self.base_url}' failed ({primary_err}). "
                f"Falling back to {FINCEPT_DEFAULT_URL}"
            )
            _rate_limit_fincept()  # honour rate limit before fallback attempt
            return self._try_url(FINCEPT_DEFAULT_URL, payload, headers)

    # ── Tool execution ────────────────────────────────────────────────────────

    def _execute_tool_call(
        self,
        tool_call: Dict[str, Any],
        functions: Dict[str, Any],
    ) -> str:
        """
        Execute a single tool call returned by the model.

        tool_call format:
          {"id": "...", "type": "function", "function": {"name": "...", "arguments": "json_str"}}
        """
        fn_info = tool_call.get("function", {})
        tool_name = fn_info.get("name", "")
        args_str = fn_info.get("arguments", "{}")

        try:
            args = json.loads(args_str) if isinstance(args_str, str) else args_str
        except Exception:
            args = {}

        fn = functions.get(tool_name)
        if fn is None:
            return f"Error: Tool '{tool_name}' not found. Available: {list(functions.keys())}"

        logger.info(f"FinceptChat: calling {tool_name}({args})")
        try:
            if hasattr(fn, "entrypoint") and callable(fn.entrypoint):
                result = fn.entrypoint(**args)
            elif callable(fn):
                result = fn(**args)
            else:
                return f"Error: '{tool_name}' is not callable"

            if result is None:
                return "Tool returned no result."
            if isinstance(result, (dict, list)):
                return json.dumps(result, indent=2, default=str)
            return str(result)
        except Exception as e:
            return f"Tool execution error for {tool_name}: {e}"

    # ── Native function-calling loop ──────────────────────────────────────────

    # Fincept endpoint hard limit is ~50k chars, but gateway times out on large prompts.
    # Keep the full prompt (query + tool context) under 20k chars to stay fast.
    _PROMPT_CHAR_BUDGET = 20_000
    # Per-tool-result cap — prevents a single large result from dominating
    _TOOL_RESULT_CAP = 2_000

    def _run_tool_loop(
        self,
        prompt: str,
        tool_schemas: List[Dict],
        functions: Dict[str, Any],
    ) -> str:
        """
        Run the native tool-calling loop:
          1. Call endpoint with tools
          2. If tool_calls in response → execute → build results context → repeat
          3. When no tool_calls → return response text

        Tool results are truncated to _TOOL_RESULT_CAP chars each, and the
        cumulative context is trimmed to stay within _PROMPT_CHAR_BUDGET.
        """
        tool_results_context = ""

        for round_num in range(self.max_tool_rounds):
            logger.debug(f"FinceptChat: tool round {round_num + 1}/{self.max_tool_rounds}")

            data = self._call_endpoint(
                prompt=prompt,
                tools=tool_schemas,
                tool_results_context=tool_results_context if tool_results_context else None,
            )

            response_text = data.get("response", "")
            tool_calls = data.get("tool_calls", [])

            logger.debug(
                f"FinceptChat: response={response_text[:100]!r}, "
                f"tool_calls={len(tool_calls)}"
            )

            if not tool_calls:
                # No more tool calls — we have the final answer
                return response_text

            # Execute all tool calls from this round
            results_parts = []
            for tc in tool_calls:
                call_id = tc.get("id", "unknown")
                fn_name = tc.get("function", {}).get("name", "unknown")
                raw_result = self._execute_tool_call(tc, functions)

                # Truncate large tool results to stay within prompt budget
                if len(raw_result) > self._TOOL_RESULT_CAP:
                    raw_result = (
                        raw_result[:self._TOOL_RESULT_CAP]
                        + f"\n... [truncated — {len(raw_result)} chars total]"
                    )

                results_parts.append(
                    f"Tool result for {fn_name} (id={call_id}):\n{raw_result}"
                )
                logger.debug(f"Tool {fn_name} result: {raw_result[:200]}")

            new_results = "\n\n".join(results_parts)

            # Build updated context and trim if over budget
            candidate = (
                (tool_results_context + "\n\n" + new_results).strip()
                if tool_results_context
                else f"Tool results:\n{new_results}"
            )
            # Keep only the most recent content if over budget
            budget = self._PROMPT_CHAR_BUDGET - len(prompt) - 500  # 500 for overhead
            if len(candidate) > budget > 0:
                candidate = candidate[-budget:]
                candidate = "... [older results trimmed]\n" + candidate
            tool_results_context = candidate

        # Hit round limit — synthesize from accumulated context
        logger.warning(
            f"FinceptChat: hit tool round limit ({self.max_tool_rounds}), "
            "requesting final synthesis"
        )
        synthesis_prompt = (
            f"{prompt}\n\n{tool_results_context}\n\n"
            "Based on the tool results above, provide a complete final answer."
        )
        data = self._call_endpoint(prompt=synthesis_prompt)
        return data.get("response", "")

    # ── Override Model.response() ─────────────────────────────────────────────

    def response(
        self,
        messages: List[Message],
        response_format=None,
        tools=None,
        tool_choice=None,
        tool_call_limit=None,
        run_response=None,
        send_media_to_model: bool = True,
        compression_manager=None,
        **kwargs,
    ) -> ModelResponse:
        """
        Override base response() to use Fincept's native function calling.

        When Agno passes Function objects via `tools`, convert them to
        Anthropic-format schemas and run the tool loop. Otherwise do a
        simple completion.
        """
        from agno.tools.function import Function

        # Build functions dict from Agno Function objects
        _functions: Dict[str, Any] = {}
        _tool_schemas: List[Dict] = []

        if tools:
            for t in tools:
                if isinstance(t, Function) and t.entrypoint:
                    _functions[t.name] = t
                    _tool_schemas.append(_function_to_anthropic_tool(t))

        # Build prompt from messages
        prompt = self._messages_to_prompt(messages)

        if _functions:
            logger.info(
                f"FinceptChat: native tool calling with "
                f"{len(_functions)} tools: {list(_functions.keys())}"
            )
            answer = self._run_tool_loop(
                prompt=prompt,
                tool_schemas=_tool_schemas,
                functions=_functions,
            )
            return ModelResponse(content=answer, role="assistant")

        # No tools — simple completion
        data = self._call_endpoint(prompt=prompt)
        content = data.get("response", str(data))
        return ModelResponse(content=content, role="assistant")

    # ── Agno Model interface ──────────────────────────────────────────────────

    def invoke(self, *args, **kwargs) -> ModelResponse:
        """Simple completion — called by base class when no tool override needed."""
        messages = args[0] if args else kwargs.get("messages", [])
        prompt = self._messages_to_prompt(messages)
        data = self._call_endpoint(prompt=prompt)
        content = data.get("response", str(data))
        return ModelResponse(content=content, role="assistant")

    async def ainvoke(self, *args, **kwargs) -> ModelResponse:
        import asyncio
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(None, lambda: self.invoke(*args, **kwargs))

    def invoke_stream(self, *args, **kwargs) -> Iterator[ModelResponse]:
        yield self.invoke(*args, **kwargs)

    async def ainvoke_stream(self, *args, **kwargs) -> AsyncIterator[ModelResponse]:
        result = await self.ainvoke(*args, **kwargs)
        yield result

    def _parse_provider_response(self, response: Any, **kwargs) -> ModelResponse:
        content = self._extract_content(response)
        return ModelResponse(content=content, role="assistant")

    def _parse_provider_response_delta(self, response: Any) -> ModelResponse:
        content = (
            self._extract_content(response)
            if isinstance(response, dict)
            else (str(response) if response else "")
        )
        return ModelResponse(content=content, role="assistant")

    # ── Helpers ───────────────────────────────────────────────────────────────

    def _messages_to_prompt(self, messages: List[Message]) -> str:
        """Convert Agno Message list to a single prompt string."""
        parts = []
        for msg in messages:
            content = msg.content or ""
            if isinstance(content, list):
                text_parts = [
                    p.get("text", "")
                    for p in content
                    if isinstance(p, dict) and p.get("type") == "text"
                ]
                content = "\n".join(text_parts) if text_parts else str(content)
            content = str(content).strip()
            if not content:
                continue
            role = getattr(msg, "role", "user") or "user"
            if role == "system":
                parts.append(f"[System]: {content}")
            elif role == "assistant":
                parts.append(f"[Assistant]: {content}")
            else:
                parts.append(content)
        return "\n\n".join(parts)

    def _extract_content(self, data: Any) -> str:
        """Extract text from various response formats."""
        if isinstance(data, dict):
            # Fincept: {"success": true, "data": {"response": "..."}}
            if "data" in data and isinstance(data["data"], dict):
                return str(data["data"].get("response", ""))
            for key in ("response", "content", "text", "answer"):
                if key in data:
                    return str(data[key])
            return str(data)
        return str(data) if data else ""

    def get_provider(self) -> str:
        return "Fincept"
