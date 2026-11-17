"""
One place that decides whether an agent run failed, and how to say so.

Agno does not raise when a model call fails (missing/invalid API key, provider
down, connection refused): it returns an ordinary RunOutput whose `status` is
ERROR and whose `content` is the error text. Every caller that turned
`response.content` into an answer without looking at the status reported that
error text as a successful result. Callers use run_error_text() before trusting
content, and raise RunFailed (or return {"success": False, ...}) on a failure.
"""

import re
from typing import Any, Optional

NO_MODEL_MESSAGE = (
    "No LLM model is configured or reachable. "
    "Choose a provider, model and API key in Settings > LLM Configuration."
)

# Error texts that mean "there is no usable model" rather than "the model
# answered badly". Matched case-insensitively against the provider's message.
_NO_MODEL_PATTERNS = re.compile(
    r"api_key not set"
    r"|api key (is )?(not set|required|missing)"
    r"|no api key"
    r"|invalid (x-)?api[ _-]?key"
    r"|incorrect api key"
    r"|could not resolve authentication method"
    r"|authentication ?error"
    r"|\bunauthori[sz]ed\b"
    r"|status code:? 401|error code:? 401|\b401\b.{0,40}api key"
    r"|connection refused"
    r"|failed to establish a new connection"
    r"|connecterror|apiconnectionerror|connection error"
    r"|name or service not known|nodename nor servname"
    r"|no llm (model|provider)",
    re.IGNORECASE,
)


class RunFailed(RuntimeError):
    """An agent/model run failed; the message is ready to show to the user."""


class AgentFeatureUnavailable(RuntimeError):
    """An agent feature the config asks for (memory, storage, knowledge, ...)
    cannot be built; the message names what is missing."""


def explain_llm_error(text: str) -> str:
    """Prefix NO_MODEL_MESSAGE when the provider error means there is no usable
    model (missing key, auth failure, endpoint unreachable). Other errors are
    returned unchanged."""
    text = (text or "").strip() or "Agent run failed"
    if text.startswith(NO_MODEL_MESSAGE):
        return text
    if _NO_MODEL_PATTERNS.search(text):
        return f"{NO_MODEL_MESSAGE} Details: {text}"
    return text


def run_error_text(response: Any) -> Optional[str]:
    """The (explained) error text when an Agno run failed, else None.

    Works for agent, team and workflow run outputs (all carry `status`).
    """
    status = getattr(response, "status", None)
    value = getattr(status, "value", status)
    if isinstance(value, str) and value.upper() == "ERROR":
        content = getattr(response, "content", None)
        return explain_llm_error(str(content) if content else "Agent run failed")
    return None


def content_or_raise(response: Any, get_content=None) -> str:
    """Text of a successful run; raises RunFailed when the run failed."""
    err = run_error_text(response)
    if err:
        raise RunFailed(err)
    if get_content is not None:
        return get_content(response)
    content = getattr(response, "content", None)
    return "" if content is None else str(content)
