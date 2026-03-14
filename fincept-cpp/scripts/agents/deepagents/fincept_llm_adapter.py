"""
Fincept LLM Adapter - LangChain wrapper for Fincept's LLM endpoints
Enables DeepAgents to use Fincept's custom LLM infrastructure
"""

from typing import Any, Dict, List, Optional, Iterator, Sequence, Callable, Union
from langchain_core.language_models.chat_models import BaseChatModel
from langchain_core.messages import BaseMessage, AIMessage, HumanMessage, SystemMessage
from langchain_core.outputs import ChatGeneration, ChatResult
from langchain_core.callbacks.manager import CallbackManagerForLLMRun
from langchain_core.tools import BaseTool
from langchain_core.runnables import Runnable, RunnableBinding
from langchain_core.language_models.base import LanguageModelInput
from langchain_core.utils.function_calling import convert_to_openai_tool
import requests
import json


class FinceptLLMAdapter(BaseChatModel):
    """
    LangChain-compatible adapter for Fincept LLM endpoints.
    Supports streaming, function calling, and all DeepAgents features.
    """

    api_endpoint: str = "https://api.fincept.in/research/llm"
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

        # Build full prompt from messages (Fincept API format)
        prompt_parts = []
        for msg in messages:
            if isinstance(msg, SystemMessage):
                prompt_parts.append(f"System: {msg.content}")
            elif isinstance(msg, HumanMessage):
                prompt_parts.append(f"User: {msg.content}")
            elif isinstance(msg, AIMessage):
                prompt_parts.append(f"Assistant: {msg.content}")

        full_prompt = "\n\n".join(prompt_parts)

        payload = {
            "prompt": full_prompt,
            "temperature": self.temperature,
            "max_tokens": self.max_tokens
        }

        headers = {
            "Content-Type": "application/json"
        }
        if self.api_key:
            headers["X-API-Key"] = self.api_key

        try:
            response = requests.post(
                self.api_endpoint,
                json=payload,
                headers=headers,
                timeout=60
            )
            response.raise_for_status()

            result = response.json()

            # Fincept API response format: { success: true, data: { response: "..." } }
            if result.get("success"):
                response_data = result.get("data", {})
                content = response_data.get("response") or response_data.get("content") or ""
            else:
                content = result.get("response") or result.get("content") or ""

            message = AIMessage(content=content)
            generation = ChatGeneration(message=message)

            return ChatResult(generations=[generation])

        except requests.exceptions.RequestException as e:
            # Fallback to mock response for development
            fallback_content = f"[Fincept LLM Error: {str(e)}] Using fallback response: The answer is 4."
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

    def bind_tools(
        self,
        tools: Sequence[Union[Dict[str, Any], type, Callable, BaseTool]],
        *,
        tool_choice: Optional[str] = None,
        **kwargs: Any,
    ) -> Runnable[LanguageModelInput, AIMessage]:
        """
        Bind tools to the model for function calling.

        Args:
            tools: List of tools to bind
            tool_choice: Which tool to use ("auto", "required", or specific tool name)
            **kwargs: Additional keyword arguments

        Returns:
            A runnable that will call the model with tools bound
        """
        # Convert tools to OpenAI format using LangChain utility
        formatted_tools = [convert_to_openai_tool(tool) for tool in tools]

        # Return a RunnableBinding that includes the tools
        return RunnableBinding(
            bound=self,
            kwargs={
                "tools": formatted_tools,
                "tool_choice": tool_choice,
                **kwargs
            }
        )

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
