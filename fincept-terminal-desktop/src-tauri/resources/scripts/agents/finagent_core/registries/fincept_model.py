"""
Fincept LLM Model - Custom adapter for Fincept's built-in LLM endpoint.

Subclasses agno's Model @dataclass to work with Agent.run().
"""

import httpx
import logging
from dataclasses import dataclass, field
from typing import Any, Optional, Dict, List, Iterator, AsyncIterator

from agno.models.base import Model
from agno.models.message import Message
from agno.models.response import ModelResponse

logger = logging.getLogger(__name__)

FINCEPT_DEFAULT_URL = "https://finceptbackend.share.zrok.io/research/llm"


@dataclass
class FinceptChat(Model):
    """
    Agno-compatible model for Fincept's built-in LLM endpoint.
    Uses @dataclass inheritance matching agno's Model base class.
    """

    id: str = "fincept-llm"
    name: str = "FinceptChat"
    provider: str = "Fincept"

    # Fincept-specific fields
    api_key: Optional[str] = None
    base_url: str = FINCEPT_DEFAULT_URL
    temperature: Optional[float] = None
    max_tokens: Optional[int] = None

    def _make_request(self, messages: List[Message]) -> Dict[str, Any]:
        """Send request to Fincept endpoint and return raw response dict."""
        # Fincept endpoint expects a single 'prompt' field, not 'messages'
        prompt = self._messages_to_prompt(messages)

        headers = {"Content-Type": "application/json"}
        if self.api_key:
            headers["X-API-Key"] = self.api_key

        payload: Dict[str, Any] = {
            "prompt": prompt,
        }
        if self.temperature is not None:
            payload["temperature"] = self.temperature
        if self.max_tokens is not None:
            payload["max_tokens"] = self.max_tokens

        try:
            with httpx.Client(timeout=120.0) as client:
                resp = client.post(self.base_url, json=payload, headers=headers)

                if resp.status_code != 200:
                    error_detail = ""
                    try:
                        err_data = resp.json()
                        error_detail = err_data.get("detail", err_data.get("message", ""))
                    except Exception:
                        error_detail = resp.text[:200]
                    raise RuntimeError(f"Fincept API error ({resp.status_code}): {error_detail}")

                return resp.json()

        except httpx.TimeoutException:
            raise RuntimeError("Fincept API request timed out (120s)")
        except httpx.ConnectError as e:
            raise RuntimeError(f"Cannot connect to Fincept API: {e}")
        except RuntimeError:
            raise
        except Exception as e:
            raise RuntimeError(f"Fincept API call failed: {e}")

    def invoke(self, *args, **kwargs) -> ModelResponse:
        """Send request to Fincept endpoint and return ModelResponse."""
        # Extract messages from args or kwargs
        messages = args[0] if args else kwargs.get("messages", [])
        raw = self._make_request(messages)
        return self._parse_provider_response(raw)

    async def ainvoke(self, *args, **kwargs) -> ModelResponse:
        """Async invoke - runs sync version in executor."""
        import asyncio
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(None, lambda: self.invoke(*args, **kwargs))

    def invoke_stream(self, *args, **kwargs) -> Iterator[ModelResponse]:
        """Streaming not supported - yield single response."""
        yield self.invoke(*args, **kwargs)

    async def ainvoke_stream(self, *args, **kwargs) -> AsyncIterator[ModelResponse]:
        """Async streaming - yields single result."""
        result = await self.ainvoke(*args, **kwargs)
        yield result

    def _parse_provider_response(self, response: Any, **kwargs) -> ModelResponse:
        """Parse raw provider response into ModelResponse."""
        content = self._extract_content(response)
        return ModelResponse(content=content, role="assistant")

    def _parse_provider_response_delta(self, response: Any) -> ModelResponse:
        """Parse streaming delta into ModelResponse."""
        content = self._extract_content(response) if isinstance(response, dict) else (str(response) if response else "")
        return ModelResponse(content=content, role="assistant")

    def _messages_to_prompt(self, messages: List[Message]) -> str:
        """Convert agno Messages list into a single prompt string for the Fincept API."""
        parts = []
        for msg in messages:
            content = msg.content or ""
            if isinstance(content, list):
                text_parts = [p.get("text", "") for p in content if isinstance(p, dict) and p.get("type") == "text"]
                content = "\n".join(text_parts) if text_parts else str(content)
            content = str(content).strip()
            if not content:
                continue

            role = msg.role or "user"
            if role == "system":
                parts.append(f"[System]: {content}")
            elif role == "user":
                parts.append(content)
            elif role == "assistant":
                parts.append(f"[Assistant]: {content}")
            else:
                parts.append(content)

        return "\n\n".join(parts)

    def _extract_content(self, data: Any) -> str:
        """Extract text content from various response formats."""
        if isinstance(data, dict):
            if "choices" in data:
                choices = data["choices"]
                if choices:
                    msg = choices[0].get("message", {})
                    return msg.get("content", "")
            if "response" in data:
                return str(data["response"])
            if "content" in data:
                return str(data["content"])
            if "text" in data:
                return str(data["text"])
            if "answer" in data:
                return str(data["answer"])
            return str(data)
        return str(data) if data else ""

    def get_provider(self) -> str:
        return "Fincept"
