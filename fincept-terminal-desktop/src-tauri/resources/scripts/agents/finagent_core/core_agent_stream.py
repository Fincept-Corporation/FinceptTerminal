"""
Core Agent Streaming - Real-time streaming responses via named pipes

Supports:
- Token-by-token streaming
- Thinking/reasoning step streaming
- Tool call notifications
- Progress updates
"""

from typing import Dict, Any, Optional, Generator, Callable
import logging
import json
import sys
import os

# Add parent directory to path for imports
from pathlib import Path
parent_dir = str(Path(__file__).parent.parent)
if parent_dir not in sys.path:
    sys.path.insert(0, parent_dir)

logger = logging.getLogger(__name__)


class StreamingCoreAgent:
    """
    Streaming version of CoreAgent.
    Yields chunks for real-time UI updates.
    """

    def __init__(
        self,
        api_keys: Optional[Dict[str, str]] = None,
        user_id: Optional[str] = None,
        stream_callback: Optional[Callable[[str, str, Optional[Dict]], None]] = None
    ):
        self.api_keys = api_keys or {}
        self.user_id = user_id
        self.stream_callback = stream_callback or self._default_callback
        self._agent = None
        self._current_config = None

    def _default_callback(self, chunk_type: str, content: str, metadata: Optional[Dict] = None):
        """Default callback - print to stdout as JSON lines"""
        chunk = {
            "type": chunk_type,
            "content": content,
            "metadata": metadata
        }
        print(json.dumps(chunk), flush=True)

    def emit(self, chunk_type: str, content: str, metadata: Optional[Dict] = None):
        """Emit a stream chunk"""
        self.stream_callback(chunk_type, content, metadata)

    def _resolve_provider_from_keys(self) -> str:
        """Derive provider from api_keys instead of defaulting to openai."""
        keys = self.api_keys or {}
        preferred = ["fincept", "ollama", "anthropic", "google", "groq",
                     "deepseek", "openai", "openrouter"]
        for provider in preferred:
            if keys.get(provider) or keys.get(f"{provider.upper()}_API_KEY"):
                return provider
        for k, v in keys.items():
            if k.endswith("_API_KEY") and v:
                return k[:-8].lower()
        return "openai"

    def run_streaming(
        self,
        query: str,
        config: Dict[str, Any],
        session_id: Optional[str] = None
    ) -> Generator[Dict[str, Any], None, None]:
        """Execute agent with streaming output."""
        from core_agent import CoreAgent

        # Emit thinking indicator
        self.emit("thinking", "Analyzing your request...")

        # Create or reuse agent
        core = CoreAgent(api_keys=self.api_keys, user_id=self.user_id)

        # Check if model supports streaming
        model_config = config.get("model", {})
        # Resolve provider from api_keys if not explicitly set in config
        provider = model_config.get("provider") or self._resolve_provider_from_keys()

        # Providers that support native streaming
        streaming_providers = ["openai", "anthropic", "groq", "together", "fireworks", "ollama",
                               "fincept", "google", "deepseek", "openrouter"]

        if provider in streaming_providers:
            # Use native streaming
            yield from self._stream_native(core, query, config, session_id)
        else:
            # Simulate streaming for non-streaming providers
            yield from self._stream_simulated(core, query, config, session_id)

    def _stream_native(
        self,
        core,
        query: str,
        config: Dict[str, Any],
        session_id: Optional[str]
    ) -> Generator[Dict[str, Any], None, None]:
        """Stream using native provider streaming support."""
        try:
            # Enable streaming in config
            config = config.copy()
            config["stream"] = True

            # Create agent with streaming
            agent = core._create_agent(config)

            # Check guardrails first
            if core._guardrails:
                self.emit("thinking", "Checking input guardrails...")
                guard_result = core.check_input(query)
                if not guard_result["passed"]:
                    self.emit("error", f"Input rejected: {guard_result['violations']}")
                    return
                query = guard_result["text"]

            self.emit("thinking", "Generating response...")

            # Run with streaming
            run_kwargs = {"input": query, "stream": True}
            if session_id:
                run_kwargs["session_id"] = session_id

            response = agent.run(**run_kwargs)

            # Handle streaming response
            full_content = ""

            if hasattr(response, '__iter__') and not isinstance(response, str):
                # Iterable response (true streaming)
                for chunk in response:
                    if hasattr(chunk, 'content'):
                        content = chunk.content
                    elif isinstance(chunk, dict):
                        content = chunk.get('content', str(chunk))
                    else:
                        content = str(chunk)

                    full_content += content
                    self.emit("token", content)
                    yield {"type": "token", "content": content}
            else:
                # Non-streaming response - emit all at once
                content = core.get_response_content(response)
                full_content = content
                # Chunk for smoother UI
                for i in range(0, len(content), 50):
                    chunk = content[i:i+50]
                    self.emit("token", chunk)
                    yield {"type": "token", "content": chunk}

            # Emit completion
            self.emit("done", full_content, {"total_tokens": len(full_content)})
            yield {"type": "done", "content": full_content}

        except Exception as e:
            self.emit("error", str(e))
            yield {"type": "error", "content": str(e)}

    def _stream_simulated(
        self,
        core,
        query: str,
        config: Dict[str, Any],
        session_id: Optional[str]
    ) -> Generator[Dict[str, Any], None, None]:
        """Simulate streaming for non-streaming providers."""
        try:
            self.emit("thinking", "Processing (non-streaming provider)...")

            # Run without streaming
            response = core.run(query, config, session_id)
            content = core.get_response_content(response)

            # Simulate streaming by chunking output
            chunk_size = 30  # Characters per chunk
            for i in range(0, len(content), chunk_size):
                chunk = content[i:i+chunk_size]
                self.emit("token", chunk)
                yield {"type": "token", "content": chunk}

            self.emit("done", content)
            yield {"type": "done", "content": content}

        except Exception as e:
            self.emit("error", str(e))
            yield {"type": "error", "content": str(e)}

    def run_team_streaming(
        self,
        query: str,
        team_config: Dict[str, Any],
        session_id: Optional[str] = None
    ) -> Generator[Dict[str, Any], None, None]:
        """Stream team execution using CoreAgent.run_team (Agno Team orchestration)."""
        try:
            from core_agent import CoreAgent

            members = team_config.get("agents", team_config.get("members", []))
            mode = team_config.get("mode", "coordinate")

            self.emit("thinking", f"Initializing team ({mode} mode, {len(members)} agents)...")

            core = CoreAgent(api_keys=self.api_keys, user_id=self.user_id)

            self.emit("thinking", "Running team via Agno Team orchestration...")

            # run_team returns an Agno TeamRunOutput object (not a dict).
            # Use get_response_content() to safely extract text — same as main.py does.
            team_response = core.run_team(query, team_config, session_id)
            content = core.get_response_content(team_response)

            if content:
                # Chunk the response for smoother streaming UI
                chunk_size = 50
                for i in range(0, len(content), chunk_size):
                    piece = content[i:i + chunk_size]
                    self.emit("token", piece)
                    yield {"type": "token", "content": piece}

                self.emit("done", content, {"agents": len(members)})
                yield {"type": "done", "content": content}
            else:
                self.emit("done", "", {"agents": len(members)})
                yield {"type": "done", "content": ""}

        except Exception as e:
            self.emit("error", str(e))
            yield {"type": "error", "content": str(e)}


def main(args=None):
    """Entry point for streaming agent execution."""
    if args is None:
        args = sys.argv[1:]

    try:
        if len(args) == 0:
            return json.dumps({"success": False, "error": "No payload provided"})

        # Parse payload
        payload = json.loads(args[0])
        action = payload.get("action", "run")
        api_keys = payload.get("api_keys", {})
        user_id = payload.get("user_id")
        config = payload.get("config", {})
        params = payload.get("params", {})
        stream_id = payload.get("stream_id")

        # Collected output for final response
        full_response = ""
        chunks = []

        def collect_callback(chunk_type: str, content: str, metadata: Optional[Dict] = None):
            """Collect chunks and print with prefixes Rust execute_with_stream_callback expects."""
            nonlocal full_response
            chunk = {
                "stream_id": stream_id,
                "type": chunk_type,
                "content": content,
                "metadata": metadata
            }
            chunks.append(chunk)
            # Escape backslashes first, then newlines so each printed line is one
            # logical chunk. Rust unescapes \n -> newline after stripping prefix.
            safe = content.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "")
            if chunk_type in ["token", "agent_token"]:
                full_response += content
                # Rust recognises TOKEN: prefix -> "token" chunk type
                print(f"TOKEN:{safe}", flush=True)
            elif chunk_type == "thinking":
                print(f"THINKING:{safe}", flush=True)
            elif chunk_type == "tool_call":
                print(f"TOOL:{safe}", flush=True)
            elif chunk_type == "tool_result":
                print(f"TOOL_RESULT:{safe}", flush=True)
            elif chunk_type in ["done", "agent_done"]:
                print(f"DONE:{safe}", flush=True)
            elif chunk_type == "error":
                print(f"ERROR:{safe}", flush=True)
            else:
                # Fallback: emit as token so Rust forwards it
                print(f"TOKEN:{safe}", flush=True)

        agent = StreamingCoreAgent(
            api_keys=api_keys,
            user_id=user_id,
            stream_callback=collect_callback
        )

        if action == "run":
            query = params.get("query")
            if not query:
                return json.dumps({"success": False, "error": "Missing query"})

            # Consume the generator
            for _ in agent.run_streaming(query, config, params.get("session_id")):
                pass

        elif action == "run_team":
            query = params.get("query")
            team_config = params.get("team_config", config)
            if not query:
                return json.dumps({"success": False, "error": "Missing query"})

            for _ in agent.run_team_streaming(query, team_config, params.get("session_id")):
                pass

        else:
            # Fallback to non-streaming core_agent
            from core_agent import main as core_main
            return core_main(args)

        return json.dumps({
            "success": True,
            "response": full_response,
            "stream_id": stream_id,
            "chunk_count": len(chunks)
        })

    except json.JSONDecodeError as e:
        return json.dumps({"success": False, "error": f"Invalid JSON: {str(e)}"})
    except Exception as e:
        return json.dumps({"success": False, "error": str(e)})


if __name__ == "__main__":
    result = main()
    print(result)
