"""
LLM Executor - Real LLM API execution for agents
Supports OpenAI, Anthropic, Google Gemini, and local Ollama
"""

import json
from typing import Dict, Any, Optional


class LLMExecutor:
    """Execute agent queries using real LLM APIs"""

    def __init__(self, api_keys: Dict[str, str]):
        self.api_keys = api_keys

    def execute(self, agent_config: Dict, query: str) -> Dict[str, Any]:
        """
        Execute agent with real LLM API

        Args:
            agent_config: Agent configuration dict
            query: User query

        Returns:
            Dict with response and metadata
        """
        llm_config = agent_config.get("llm_config", {})
        provider = llm_config.get("provider", "openai").lower()
        model = llm_config.get("model_id", "gpt-4-turbo")
        temperature = llm_config.get("temperature", 0.7)
        max_tokens = llm_config.get("max_tokens", 2000)
        instructions = agent_config.get("instructions", "")

        # Build system prompt
        system_prompt = self._build_system_prompt(agent_config, instructions)

        try:
            if provider == "openai":
                return self._execute_openai(model, system_prompt, query, temperature, max_tokens)
            elif provider == "anthropic":
                return self._execute_anthropic(model, system_prompt, query, temperature, max_tokens)
            elif provider in ["google", "gemini"]:
                return self._execute_google(model, system_prompt, query, temperature, max_tokens)
            elif provider == "ollama":
                return self._execute_ollama(model, system_prompt, query, temperature, max_tokens)
            elif provider == "fincept":
                return self._execute_fincept(model, system_prompt, query, temperature, max_tokens)
            elif provider == "openrouter":
                return self._execute_openrouter(model, system_prompt, query, temperature, max_tokens)
            elif provider == "deepseek":
                return self._execute_deepseek(model, system_prompt, query, temperature, max_tokens)
            elif provider == "groq":
                return self._execute_groq(model, system_prompt, query, temperature, max_tokens)
            else:
                # Generic OpenAI-compatible API fallback
                return self._execute_generic_openai_compatible(provider, model, system_prompt, query, temperature, max_tokens)
        except Exception as e:
            return {
                "success": False,
                "error": f"LLM execution failed: {str(e)}"
            }

    def _build_system_prompt(self, agent_config: Dict, instructions: str) -> str:
        """Build comprehensive system prompt from agent config"""
        parts = []

        # Basic role
        role = agent_config.get("role", "")
        goal = agent_config.get("goal", "")
        if role:
            parts.append(f"Role: {role}")
        if goal:
            parts.append(f"Goal: {goal}")

        # Instructions
        if instructions:
            parts.append(f"\nInstructions:\n{instructions}")

        # Add scoring weights if available (for transparency)
        if "scoring_weights" in agent_config:
            parts.append("\nScoring Weights:")
            for key, value in agent_config["scoring_weights"].items():
                parts.append(f"  - {key}: {value*100:.0f}%")

        # Add thresholds if available
        if "thresholds" in agent_config:
            parts.append("\nAnalysis Thresholds:")
            for key, value in agent_config["thresholds"].items():
                parts.append(f"  - {key}: {value}")

        return "\n".join(parts)

    def _execute_openai(self, model: str, system_prompt: str, query: str,
                        temperature: float, max_tokens: int) -> Dict:
        """Execute with OpenAI API"""
        try:
            from openai import OpenAI
        except ImportError:
            return {"success": False, "error": "openai package not installed"}

        api_key = self.api_keys.get("OPENAI_API_KEY")
        if not api_key:
            available_keys = [k for k in self.api_keys.keys() if k.endswith('_API_KEY')]
            suggestion = f" Available keys: {', '.join(available_keys)}" if available_keys else ""
            return {"success": False, "error": f"OPENAI_API_KEY not configured in Settings.{suggestion} Change agent's provider in JSON config or add OpenAI key in Settings."}

        try:
            client = OpenAI(api_key=api_key)

            # Use max_completion_tokens for newer models (GPT-4o, o1, etc)
            params = {
                "model": model,
                "messages": [
                    {"role": "system", "content": system_prompt},
                    {"role": "user", "content": query}
                ],
                "temperature": temperature
            }

            # OpenAI changed to max_completion_tokens for all newer models
            # Try max_completion_tokens first (works for gpt-4o, o1, o3, gpt-4-turbo-2024+)
            # Fall back to max_tokens for legacy models
            try:
                params["max_completion_tokens"] = max_tokens
                response = client.chat.completions.create(**params)
            except Exception as first_error:
                # If max_completion_tokens fails, try max_tokens
                if "max_completion_tokens" in str(first_error) or "unsupported_parameter" in str(first_error).lower():
                    del params["max_completion_tokens"]
                    params["max_tokens"] = max_tokens
                    response = client.chat.completions.create(**params)
                else:
                    raise first_error

            return {
                "success": True,
                "response": response.choices[0].message.content,
                "provider": "openai",
                "model": model,
                "tokens_used": response.usage.total_tokens if response.usage else 0,
                "finish_reason": response.choices[0].finish_reason
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"OpenAI API error: {str(e)}"
            }

    def _execute_anthropic(self, model: str, system_prompt: str, query: str,
                           temperature: float, max_tokens: int) -> Dict:
        """Execute with Anthropic Claude API"""
        try:
            from anthropic import Anthropic
        except ImportError:
            return {"success": False, "error": "anthropic package not installed"}

        api_key = self.api_keys.get("ANTHROPIC_API_KEY")
        if not api_key:
            return {"success": False, "error": "ANTHROPIC_API_KEY not configured in Settings"}

        try:
            client = Anthropic(api_key=api_key)

            response = client.messages.create(
                model=model,
                max_tokens=max_tokens,
                temperature=temperature,
                system=system_prompt,
                messages=[
                    {"role": "user", "content": query}
                ]
            )

            return {
                "success": True,
                "response": response.content[0].text,
                "provider": "anthropic",
                "model": model,
                "tokens_used": response.usage.input_tokens + response.usage.output_tokens,
                "finish_reason": response.stop_reason
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Anthropic API error: {str(e)}"
            }

    def _execute_google(self, model: str, system_prompt: str, query: str,
                        temperature: float, max_tokens: int) -> Dict:
        """Execute with Google Gemini API"""
        try:
            import google.generativeai as genai
        except ImportError:
            return {"success": False, "error": "google-generativeai package not installed"}

        api_key = self.api_keys.get("GOOGLE_API_KEY") or self.api_keys.get("GEMINI_API_KEY")
        if not api_key:
            return {"success": False, "error": "GOOGLE_API_KEY not configured in Settings"}

        try:
            genai.configure(api_key=api_key)

            model_instance = genai.GenerativeModel(model)

            # Combine system prompt and query for Gemini
            full_prompt = f"{system_prompt}\n\nUser Query: {query}"

            response = model_instance.generate_content(
                full_prompt,
                generation_config={
                    "temperature": temperature,
                    "max_output_tokens": max_tokens,
                }
            )

            return {
                "success": True,
                "response": response.text,
                "provider": "google",
                "model": model,
                "tokens_used": 0,  # Gemini doesn't always return token counts
                "finish_reason": "complete"
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Google Gemini API error: {str(e)}"
            }

    def _execute_ollama(self, model: str, system_prompt: str, query: str,
                        temperature: float, max_tokens: int) -> Dict:
        """Execute with local Ollama"""
        try:
            import requests
        except ImportError:
            return {"success": False, "error": "requests package not installed"}

        ollama_url = "http://localhost:11434/api/generate"

        try:
            payload = {
                "model": model,
                "prompt": f"{system_prompt}\n\nUser: {query}\nAssistant:",
                "stream": False,
                "options": {
                    "temperature": temperature,
                    "num_predict": max_tokens
                }
            }

            response = requests.post(ollama_url, json=payload, timeout=120)
            response.raise_for_status()

            result = response.json()

            return {
                "success": True,
                "response": result.get("response", ""),
                "provider": "ollama",
                "model": model,
                "tokens_used": result.get("eval_count", 0),
                "finish_reason": "complete"
            }
        except requests.exceptions.ConnectionError:
            return {
                "success": False,
                "error": "Ollama not running. Start Ollama server: ollama serve"
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Ollama error: {str(e)}"
            }

    def _execute_fincept(self, model: str, system_prompt: str, query: str,
                        temperature: float, max_tokens: int) -> Dict:
        """Execute with Fincept LLM API"""
        try:
            import requests
        except ImportError:
            return {"success": False, "error": "requests package not installed"}

        api_key = self.api_keys.get("FINCEPT_API_KEY")
        if not api_key:
            return {"success": False, "error": "FINCEPT_API_KEY not configured in Settings"}

        fincept_url = "https://finceptbackend.share.zrok.io/research/llm"

        try:
            # Combine system prompt and query into single prompt
            full_prompt = f"{system_prompt}\n\nUser Query: {query}"

            payload = {
                "prompt": full_prompt,
                "temperature": temperature,
                "max_tokens": max_tokens
            }

            headers = {
                "X-API-Key": api_key,
                "Content-Type": "application/json"
            }

            response = requests.post(fincept_url, json=payload, headers=headers, timeout=60)
            response.raise_for_status()

            result = response.json()

            # Fincept API returns nested response: {"success": true, "data": {"response": "...", ...}}
            if result.get("success") and result.get("data"):
                data = result.get("data", {})
                llm_response = data.get("response", "")
                tokens = data.get("usage", {}).get("total_tokens", 0) if isinstance(data.get("usage"), dict) else 0

                return {
                    "success": True,
                    "response": llm_response,
                    "provider": "fincept",
                    "model": data.get("model", model),
                    "tokens_used": tokens,
                    "finish_reason": "complete"
                }
            else:
                return {
                    "success": False,
                    "error": result.get("message", "Fincept API returned unexpected format")
                }
        except requests.exceptions.ConnectionError:
            return {
                "success": False,
                "error": "Cannot connect to Fincept API. Check internet connection."
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Fincept API error: {str(e)}"
            }

    def _execute_openrouter(self, model: str, system_prompt: str, query: str,
                           temperature: float, max_tokens: int) -> Dict:
        """Execute with OpenRouter API (OpenAI-compatible)"""
        try:
            from openai import OpenAI
        except ImportError:
            return {"success": False, "error": "openai package not installed"}

        api_key = self.api_keys.get("OPENROUTER_API_KEY")
        if not api_key:
            return {"success": False, "error": "OPENROUTER_API_KEY not configured in Settings"}

        try:
            client = OpenAI(
                base_url="https://openrouter.ai/api/v1",
                api_key=api_key
            )

            response = client.chat.completions.create(
                model=model,
                messages=[
                    {"role": "system", "content": system_prompt},
                    {"role": "user", "content": query}
                ],
                temperature=temperature,
                max_tokens=max_tokens
            )

            return {
                "success": True,
                "response": response.choices[0].message.content,
                "provider": "openrouter",
                "model": model,
                "tokens_used": response.usage.total_tokens if response.usage else 0,
                "finish_reason": response.choices[0].finish_reason
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"OpenRouter API error: {str(e)}"
            }

    def _execute_deepseek(self, model: str, system_prompt: str, query: str,
                         temperature: float, max_tokens: int) -> Dict:
        """Execute with DeepSeek API (OpenAI-compatible)"""
        try:
            from openai import OpenAI
        except ImportError:
            return {"success": False, "error": "openai package not installed"}

        api_key = self.api_keys.get("DEEPSEEK_API_KEY")
        if not api_key:
            return {"success": False, "error": "DEEPSEEK_API_KEY not configured in Settings"}

        try:
            client = OpenAI(
                base_url="https://api.deepseek.com",
                api_key=api_key
            )

            response = client.chat.completions.create(
                model=model,
                messages=[
                    {"role": "system", "content": system_prompt},
                    {"role": "user", "content": query}
                ],
                temperature=temperature,
                max_tokens=max_tokens
            )

            return {
                "success": True,
                "response": response.choices[0].message.content,
                "provider": "deepseek",
                "model": model,
                "tokens_used": response.usage.total_tokens if response.usage else 0,
                "finish_reason": response.choices[0].finish_reason
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"DeepSeek API error: {str(e)}"
            }

    def _execute_groq(self, model: str, system_prompt: str, query: str,
                     temperature: float, max_tokens: int) -> Dict:
        """Execute with Groq API (OpenAI-compatible)"""
        try:
            from openai import OpenAI
        except ImportError:
            return {"success": False, "error": "openai package not installed"}

        api_key = self.api_keys.get("GROQ_API_KEY")
        if not api_key:
            return {"success": False, "error": "GROQ_API_KEY not configured in Settings"}

        try:
            client = OpenAI(
                base_url="https://api.groq.com/openai/v1",
                api_key=api_key
            )

            response = client.chat.completions.create(
                model=model,
                messages=[
                    {"role": "system", "content": system_prompt},
                    {"role": "user", "content": query}
                ],
                temperature=temperature,
                max_tokens=max_tokens
            )

            return {
                "success": True,
                "response": response.choices[0].message.content,
                "provider": "groq",
                "model": model,
                "tokens_used": response.usage.total_tokens if response.usage else 0,
                "finish_reason": response.choices[0].finish_reason
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Groq API error: {str(e)}"
            }

    def _execute_generic_openai_compatible(self, provider: str, model: str, system_prompt: str,
                                          query: str, temperature: float, max_tokens: int) -> Dict:
        """Execute with any OpenAI-compatible API using base_url from settings"""
        try:
            from openai import OpenAI
        except ImportError:
            return {"success": False, "error": "openai package not installed"}

        api_key_name = f"{provider.upper()}_API_KEY"
        api_key = self.api_keys.get(api_key_name)

        if not api_key:
            return {
                "success": False,
                "error": f"{api_key_name} not configured in Settings. Add API key or change provider."
            }

        # Try to get base_url from environment or use provider as base URL hint
        base_url_key = f"{provider.upper()}_BASE_URL"
        base_url = self.api_keys.get(base_url_key)

        if not base_url:
            return {
                "success": False,
                "error": f"Provider '{provider}' requires BASE_URL. Configure {base_url_key} in Settings or use a supported provider."
            }

        try:
            client = OpenAI(
                base_url=base_url,
                api_key=api_key
            )

            response = client.chat.completions.create(
                model=model,
                messages=[
                    {"role": "system", "content": system_prompt},
                    {"role": "user", "content": query}
                ],
                temperature=temperature,
                max_tokens=max_tokens
            )

            return {
                "success": True,
                "response": response.choices[0].message.content,
                "provider": provider,
                "model": model,
                "tokens_used": response.usage.total_tokens if response.usage else 0,
                "finish_reason": response.choices[0].finish_reason
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"{provider} API error: {str(e)}"
            }
