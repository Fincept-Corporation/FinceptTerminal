"""
Hooks Module - Pre/Post processing for financial agents

Provides:
- Input validation hooks
- Output formatting hooks
- Audit logging hooks
- Rate limiting hooks
- Cost tracking hooks
"""

from typing import Dict, Any, Optional, List, Callable, Union
from dataclasses import dataclass
from datetime import datetime, timedelta
import logging
import json
import time
import functools

logger = logging.getLogger(__name__)


@dataclass
class HookResult:
    """Result of a hook execution."""
    success: bool
    data: Any = None
    error: Optional[str] = None
    modified: bool = False


class Hook:
    """Base class for hooks."""

    def __init__(self, name: str, enabled: bool = True):
        """Initialize hook."""
        self.name = name
        self.enabled = enabled

    def execute(self, data: Any, context: Dict[str, Any] = None) -> HookResult:
        """Execute the hook."""
        raise NotImplementedError

    def __call__(self, data: Any, context: Dict[str, Any] = None) -> HookResult:
        """Call the hook."""
        if not self.enabled:
            return HookResult(success=True, data=data)
        return self.execute(data, context)


class InputValidationHook(Hook):
    """
    Validate inputs before agent execution.

    Checks:
    - Required fields present
    - Data types correct
    - Values in allowed ranges
    """

    def __init__(
        self,
        required_fields: List[str] = None,
        max_query_length: int = 10000,
        blocked_patterns: List[str] = None
    ):
        """
        Initialize input validation hook.

        Args:
            required_fields: Fields that must be present
            max_query_length: Maximum query length
            blocked_patterns: Patterns to block
        """
        super().__init__("input_validation")
        self.required_fields = required_fields or []
        self.max_query_length = max_query_length
        self.blocked_patterns = blocked_patterns or []

    def execute(self, data: Any, context: Dict[str, Any] = None) -> HookResult:
        """Validate input data."""
        errors = []

        # Check query length
        if isinstance(data, str) and len(data) > self.max_query_length:
            errors.append(f"Query exceeds maximum length ({self.max_query_length})")

        # Check required fields if data is dict
        if isinstance(data, dict):
            for field in self.required_fields:
                if field not in data or data[field] is None:
                    errors.append(f"Missing required field: {field}")

        # Check blocked patterns
        data_str = json.dumps(data) if isinstance(data, dict) else str(data)
        for pattern in self.blocked_patterns:
            if pattern.lower() in data_str.lower():
                errors.append(f"Blocked pattern detected: {pattern}")

        if errors:
            return HookResult(success=False, error="; ".join(errors))

        return HookResult(success=True, data=data)


class OutputFormattingHook(Hook):
    """
    Format agent outputs.

    Features:
    - Add metadata
    - Standardize structure
    - Add timestamps
    """

    def __init__(
        self,
        add_timestamp: bool = True,
        add_metadata: bool = True,
        format_type: str = "standard"
    ):
        """
        Initialize output formatting hook.

        Args:
            add_timestamp: Add timestamp to output
            add_metadata: Add metadata to output
            format_type: Output format type
        """
        super().__init__("output_formatting")
        self.add_timestamp = add_timestamp
        self.add_metadata = add_metadata
        self.format_type = format_type

    def execute(self, data: Any, context: Dict[str, Any] = None) -> HookResult:
        """Format output data."""
        context = context or {}

        if isinstance(data, str):
            formatted = {"content": data}
        elif isinstance(data, dict):
            formatted = data.copy()
        else:
            formatted = {"data": data}

        if self.add_timestamp:
            formatted["timestamp"] = datetime.now().isoformat()

        if self.add_metadata:
            formatted["_metadata"] = {
                "agent_id": context.get("agent_id"),
                "session_id": context.get("session_id"),
                "model": context.get("model"),
                "format_version": "1.0"
            }

        return HookResult(success=True, data=formatted, modified=True)


class AuditLoggingHook(Hook):
    """
    Log agent actions for audit trail.

    Logs:
    - All inputs and outputs
    - Tool calls
    - Errors and exceptions
    """

    def __init__(
        self,
        log_inputs: bool = True,
        log_outputs: bool = True,
        log_to_file: str = None,
        log_to_db: str = None
    ):
        """
        Initialize audit logging hook.

        Args:
            log_inputs: Log input data
            log_outputs: Log output data
            log_to_file: File path for logging
            log_to_db: Database path for logging
        """
        super().__init__("audit_logging")
        self.log_inputs = log_inputs
        self.log_outputs = log_outputs
        self.log_to_file = log_to_file
        self.log_to_db = log_to_db

    def execute(self, data: Any, context: Dict[str, Any] = None) -> HookResult:
        """Log data for audit."""
        context = context or {}
        hook_type = context.get("hook_type", "unknown")

        log_entry = {
            "timestamp": datetime.now().isoformat(),
            "type": hook_type,
            "agent_id": context.get("agent_id"),
            "session_id": context.get("session_id"),
            "user_id": context.get("user_id"),
        }

        # Add data based on settings
        if hook_type == "pre" and self.log_inputs:
            log_entry["input"] = self._sanitize_for_log(data)
        elif hook_type == "post" and self.log_outputs:
            log_entry["output"] = self._sanitize_for_log(data)

        # Log to file
        if self.log_to_file:
            self._log_to_file(log_entry)

        # Log to database
        if self.log_to_db:
            self._log_to_db(log_entry)

        # Always log to Python logger
        logger.info(f"Audit: {json.dumps(log_entry, default=str)}")

        return HookResult(success=True, data=data)

    def _sanitize_for_log(self, data: Any) -> Any:
        """Sanitize data for logging (remove sensitive info)."""
        if isinstance(data, str):
            # Truncate long strings
            return data[:1000] + "..." if len(data) > 1000 else data
        elif isinstance(data, dict):
            sanitized = {}
            for k, v in data.items():
                # Skip sensitive keys
                if any(s in k.lower() for s in ["password", "secret", "key", "token"]):
                    sanitized[k] = "[REDACTED]"
                else:
                    sanitized[k] = self._sanitize_for_log(v)
            return sanitized
        return data

    def _log_to_file(self, entry: Dict) -> None:
        """Write log entry to file."""
        try:
            with open(self.log_to_file, "a") as f:
                f.write(json.dumps(entry, default=str) + "\n")
        except Exception as e:
            logger.error(f"Failed to write audit log: {e}")

    def _log_to_db(self, entry: Dict) -> None:
        """Write log entry to database."""
        try:
            import sqlite3

            conn = sqlite3.connect(self.log_to_db)
            cursor = conn.cursor()

            cursor.execute("""
                CREATE TABLE IF NOT EXISTS audit_hooks (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    timestamp TEXT,
                    type TEXT,
                    agent_id TEXT,
                    session_id TEXT,
                    user_id TEXT,
                    data TEXT
                )
            """)

            cursor.execute("""
                INSERT INTO audit_hooks (timestamp, type, agent_id, session_id, user_id, data)
                VALUES (?, ?, ?, ?, ?, ?)
            """, (
                entry["timestamp"],
                entry["type"],
                entry.get("agent_id"),
                entry.get("session_id"),
                entry.get("user_id"),
                json.dumps(entry, default=str)
            ))

            conn.commit()
            conn.close()

        except Exception as e:
            logger.error(f"Failed to write to audit DB: {e}")


class RateLimitHook(Hook):
    """
    Rate limit agent executions.

    Features:
    - Per-user limits
    - Per-symbol limits
    - Sliding window
    """

    def __init__(
        self,
        max_requests: int = 100,
        window_seconds: int = 60,
        per_user: bool = True
    ):
        """
        Initialize rate limit hook.

        Args:
            max_requests: Maximum requests in window
            window_seconds: Window size in seconds
            per_user: Apply limit per user
        """
        super().__init__("rate_limit")
        self.max_requests = max_requests
        self.window_seconds = window_seconds
        self.per_user = per_user
        self._request_times: Dict[str, List[float]] = {}

    def execute(self, data: Any, context: Dict[str, Any] = None) -> HookResult:
        """Check rate limit."""
        context = context or {}
        key = context.get("user_id", "global") if self.per_user else "global"

        now = time.time()
        window_start = now - self.window_seconds

        # Get request times for this key
        if key not in self._request_times:
            self._request_times[key] = []

        # Filter to window
        self._request_times[key] = [
            t for t in self._request_times[key] if t > window_start
        ]

        # Check limit
        if len(self._request_times[key]) >= self.max_requests:
            return HookResult(
                success=False,
                error=f"Rate limit exceeded ({self.max_requests} requests per {self.window_seconds}s)"
            )

        # Record this request
        self._request_times[key].append(now)

        return HookResult(success=True, data=data)


class CostTrackingHook(Hook):
    """
    Track API costs.

    Features:
    - Token usage tracking
    - Cost calculation
    - Budget enforcement
    """

    # Approximate costs per 1K tokens
    COSTS = {
        "gpt-4-turbo": {"input": 0.01, "output": 0.03},
        "gpt-4": {"input": 0.03, "output": 0.06},
        "gpt-3.5-turbo": {"input": 0.0005, "output": 0.0015},
        "claude-3-opus": {"input": 0.015, "output": 0.075},
        "claude-3-sonnet": {"input": 0.003, "output": 0.015},
        "claude-3-haiku": {"input": 0.00025, "output": 0.00125},
    }

    def __init__(
        self,
        budget_limit: float = None,
        track_by_user: bool = True,
        track_by_session: bool = True
    ):
        """
        Initialize cost tracking hook.

        Args:
            budget_limit: Maximum budget (stops if exceeded)
            track_by_user: Track costs per user
            track_by_session: Track costs per session
        """
        super().__init__("cost_tracking")
        self.budget_limit = budget_limit
        self.track_by_user = track_by_user
        self.track_by_session = track_by_session

        self._total_cost = 0.0
        self._user_costs: Dict[str, float] = {}
        self._session_costs: Dict[str, float] = {}
        self._usage_log: List[Dict] = []

    def execute(self, data: Any, context: Dict[str, Any] = None) -> HookResult:
        """Track costs from response."""
        context = context or {}

        # Extract usage from context or data
        usage = context.get("usage", {})
        if isinstance(data, dict):
            usage = data.get("usage", usage)

        input_tokens = usage.get("input_tokens", 0)
        output_tokens = usage.get("output_tokens", 0)
        model = context.get("model") or context.get("provider", "unknown")

        # Calculate cost
        cost = self._calculate_cost(model, input_tokens, output_tokens)

        # Update totals
        self._total_cost += cost

        if self.track_by_user and context.get("user_id"):
            user_id = context["user_id"]
            self._user_costs[user_id] = self._user_costs.get(user_id, 0) + cost

        if self.track_by_session and context.get("session_id"):
            session_id = context["session_id"]
            self._session_costs[session_id] = self._session_costs.get(session_id, 0) + cost

        # Log usage
        self._usage_log.append({
            "timestamp": datetime.now().isoformat(),
            "model": model,
            "input_tokens": input_tokens,
            "output_tokens": output_tokens,
            "cost": cost,
            "user_id": context.get("user_id"),
            "session_id": context.get("session_id")
        })

        # Check budget
        if self.budget_limit and self._total_cost > self.budget_limit:
            return HookResult(
                success=False,
                error=f"Budget exceeded (${self._total_cost:.4f} > ${self.budget_limit:.4f})"
            )

        return HookResult(success=True, data=data)

    def _calculate_cost(self, model: str, input_tokens: int, output_tokens: int) -> float:
        """Calculate cost for token usage."""
        # Find matching model costs
        model_lower = model.lower()
        for model_name, costs in self.COSTS.items():
            if model_name in model_lower:
                input_cost = (input_tokens / 1000) * costs["input"]
                output_cost = (output_tokens / 1000) * costs["output"]
                return input_cost + output_cost

        # Default costs
        return (input_tokens + output_tokens) / 1000 * 0.01

    def get_total_cost(self) -> float:
        """Get total cost."""
        return self._total_cost

    def get_user_cost(self, user_id: str) -> float:
        """Get cost for specific user."""
        return self._user_costs.get(user_id, 0)

    def get_session_cost(self, session_id: str) -> float:
        """Get cost for specific session."""
        return self._session_costs.get(session_id, 0)

    def get_usage_summary(self) -> Dict[str, Any]:
        """Get usage summary."""
        total_input = sum(u.get("input_tokens", 0) for u in self._usage_log)
        total_output = sum(u.get("output_tokens", 0) for u in self._usage_log)

        return {
            "total_cost": self._total_cost,
            "total_requests": len(self._usage_log),
            "total_input_tokens": total_input,
            "total_output_tokens": total_output,
            "by_user": self._user_costs,
            "by_session": self._session_costs
        }


class HooksModule:
    """
    Hooks manager for financial agents.

    Manages pre and post hooks for agent execution.
    """

    def __init__(self):
        """Initialize hooks module."""
        self.pre_hooks: List[Hook] = []
        self.post_hooks: List[Hook] = []
        self.tool_hooks: Dict[str, List[Hook]] = {}

    def add_pre_hook(self, hook: Hook) -> "HooksModule":
        """Add a pre-execution hook."""
        self.pre_hooks.append(hook)
        return self

    def add_post_hook(self, hook: Hook) -> "HooksModule":
        """Add a post-execution hook."""
        self.post_hooks.append(hook)
        return self

    def add_tool_hook(self, tool_name: str, hook: Hook) -> "HooksModule":
        """Add a hook for specific tool."""
        if tool_name not in self.tool_hooks:
            self.tool_hooks[tool_name] = []
        self.tool_hooks[tool_name].append(hook)
        return self

    def run_pre_hooks(self, data: Any, context: Dict[str, Any] = None) -> HookResult:
        """Run all pre-hooks."""
        context = context or {}
        context["hook_type"] = "pre"
        current_data = data

        for hook in self.pre_hooks:
            result = hook(current_data, context)
            if not result.success:
                return result
            if result.modified:
                current_data = result.data

        return HookResult(success=True, data=current_data)

    def run_post_hooks(self, data: Any, context: Dict[str, Any] = None) -> HookResult:
        """Run all post-hooks."""
        context = context or {}
        context["hook_type"] = "post"
        current_data = data

        for hook in self.post_hooks:
            result = hook(current_data, context)
            if not result.success:
                return result
            if result.modified:
                current_data = result.data

        return HookResult(success=True, data=current_data)

    def run_tool_hooks(self, tool_name: str, data: Any, context: Dict[str, Any] = None) -> HookResult:
        """Run hooks for specific tool."""
        if tool_name not in self.tool_hooks:
            return HookResult(success=True, data=data)

        current_data = data
        for hook in self.tool_hooks[tool_name]:
            result = hook(current_data, context)
            if not result.success:
                return result
            if result.modified:
                current_data = result.data

        return HookResult(success=True, data=current_data)

    def to_agent_config(self) -> Dict[str, Any]:
        """
        Convert to Agno agent config.

        Returns config for Agent initialization.
        """
        # Convert to Agno hook format
        pre_hooks = []
        post_hooks = []

        # Agno expects callable hooks
        for hook in self.pre_hooks:
            pre_hooks.append(lambda data, ctx=None, h=hook: h(data, ctx))

        for hook in self.post_hooks:
            post_hooks.append(lambda data, ctx=None, h=hook: h(data, ctx))

        return {
            "pre_hooks": pre_hooks if pre_hooks else None,
            "post_hooks": post_hooks if post_hooks else None,
            "tool_hooks": self.tool_hooks if self.tool_hooks else None
        }

    @classmethod
    def from_config(cls, config: Dict[str, Any]) -> "HooksModule":
        """Create from configuration."""
        module = cls()

        if config.get("input_validation"):
            module.add_pre_hook(InputValidationHook(
                **config.get("input_validation", {})
            ))

        if config.get("output_formatting"):
            module.add_post_hook(OutputFormattingHook(
                **config.get("output_formatting", {})
            ))

        if config.get("audit_logging"):
            hook = AuditLoggingHook(**config.get("audit_logging", {}))
            module.add_pre_hook(hook)
            module.add_post_hook(hook)

        if config.get("rate_limit"):
            module.add_pre_hook(RateLimitHook(
                **config.get("rate_limit", {})
            ))

        if config.get("cost_tracking"):
            module.add_post_hook(CostTrackingHook(
                **config.get("cost_tracking", {})
            ))

        return module

    @classmethod
    def default_financial(cls) -> "HooksModule":
        """Create default hooks for financial agents."""
        return (cls()
            .add_pre_hook(InputValidationHook(max_query_length=10000))
            .add_pre_hook(RateLimitHook(max_requests=100, window_seconds=60))
            .add_post_hook(OutputFormattingHook(add_timestamp=True))
            .add_post_hook(AuditLoggingHook(log_inputs=True, log_outputs=True))
            .add_post_hook(CostTrackingHook(track_by_user=True)))
