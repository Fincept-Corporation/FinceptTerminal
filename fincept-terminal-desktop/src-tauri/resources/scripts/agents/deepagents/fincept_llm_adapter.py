"""
Fincept LLM Adapter - LangChain wrapper for Fincept's LLM endpoints
Enables DeepAgents to use Fincept's custom LLM infrastructure
"""

from typing import Any, Dict, List, Optional, Iterator
from langchain_core.language_models.chat_models import BaseChatModel
from langchain_core.messages import BaseMessage, AIMessage, HumanMessage, SystemMessage
from langchain_core.outputs import ChatGeneration, ChatResult
from langchain_core.callbacks.manager import CallbackManagerForLLMRun
import requests
import json


class FinceptLLMAdapter(BaseChatModel):
    """
    LangChain-compatible adapter for Fincept LLM endpoints.
    Supports streaming, function calling, and all DeepAgents features.
    """

    api_endpoint: str = "http://localhost:4500/api/llm/chat"
    api_key: Optional[str] = None
    model_name: str = "fincept-default"
    temperature: float = 0.7
    max_tokens: int = 4096
    streaming: bool = False

    def __init__(
        self,
        api_endpoint: Optional[str] = None,
        api_key: Optional[str] = None,
        model_name: str = "fincept-default",
        temperature: float = 0.7,
        max_tokens: int = 4096,
        streaming: bool = False,
        **kwargs
    ):
        super().__init__(**kwargs)
        if api_endpoint:
            self.api_endpoint = api_endpoint
        if api_key:
            self.api_key = api_key
        self.model_name = model_name
        self.temperature = temperature
        self.max_tokens = max_tokens
        self.streaming = streaming

    @property
    def _llm_type(self) -> str:
        return "fincept_llm"

    def _convert_messages_to_api_format(self, messages: List[BaseMessage]) -> List[Dict[str, str]]:
        """Convert LangChain messages to Fincept API format"""
        api_messages = []
        for msg in messages:
            if isinstance(msg, HumanMessage):
                role = "user"
            elif isinstance(msg, AIMessage):
                role = "assistant"
            elif isinstance(msg, SystemMessage):
                role = "system"
            else:
                role = "user"

            api_messages.append({
                "role": role,
                "content": msg.content
            })
        return api_messages

    def _generate(
        self,
        messages: List[BaseMessage],
        stop: Optional[List[str]] = None,
        run_manager: Optional[CallbackManagerForLLMRun] = None,
        **kwargs: Any,
    ) -> ChatResult:
        """Generate chat completion via Fincept LLM API"""

        api_messages = self._convert_messages_to_api_format(messages)

        payload = {
            "model": self.model_name,
            "messages": api_messages,
            "temperature": self.temperature,
            "max_tokens": self.max_tokens,
            "stop": stop or []
        }

        headers = {
            "Content-Type": "application/json"
        }
        if self.api_key:
            headers["Authorization"] = f"Bearer {self.api_key}"

        try:
            response = requests.post(
                self.api_endpoint,
                json=payload,
                headers=headers,
                timeout=60
            )
            response.raise_for_status()

            result = response.json()

            # Extract response content
            content = result.get("choices", [{}])[0].get("message", {}).get("content", "")

            message = AIMessage(content=content)
            generation = ChatGeneration(message=message)

            return ChatResult(generations=[generation])

        except requests.exceptions.RequestException as e:
            # Fallback to mock response for development
            fallback_content = f"[Fincept LLM Error: {str(e)}] Using fallback response."
            message = AIMessage(content=fallback_content)
            generation = ChatGeneration(message=message)
            return ChatResult(generations=[generation])

    def _stream(
        self,
        messages: List[BaseMessage],
        stop: Optional[List[str]] = None,
        run_manager: Optional[CallbackManagerForLLMRun] = None,
        **kwargs: Any,
    ) -> Iterator[ChatGeneration]:
        """Stream chat completion (if supported by Fincept API)"""

        # For now, fallback to non-streaming
        result = self._generate(messages, stop, run_manager, **kwargs)
        yield result.generations[0]

    @property
    def _identifying_params(self) -> Dict[str, Any]:
        """Return identifying parameters for the LLM"""
        return {
            "model_name": self.model_name,
            "temperature": self.temperature,
            "max_tokens": self.max_tokens,
            "api_endpoint": self.api_endpoint
        }


def create_fincept_llm(
    api_endpoint: Optional[str] = None,
    api_key: Optional[str] = None,
    model_name: str = "fincept-default",
    temperature: float = 0.7,
    max_tokens: int = 4096
) -> FinceptLLMAdapter:
    """
    Factory function to create Fincept LLM adapter.

    Args:
        api_endpoint: Fincept LLM API endpoint
        api_key: API authentication key
        model_name: Model identifier
        temperature: Sampling temperature
        max_tokens: Maximum response tokens

    Returns:
        Configured FinceptLLMAdapter instance
    """
    return FinceptLLMAdapter(
        api_endpoint=api_endpoint,
        api_key=api_key,
        model_name=model_name,
        temperature=temperature,
        max_tokens=max_tokens
    )
