"""
Reasoning Module - Step-by-step reasoning and chain-of-thought

Provides:
- Structured reasoning steps
- Chain-of-thought prompting
- Reasoning event handling
- Step tracking and debugging
"""

from typing import Dict, Any, Optional, List, Callable
import logging

logger = logging.getLogger(__name__)


class ReasoningModule:
    """
    Step-by-step reasoning for complex tasks.

    Supports:
    - Chain-of-thought reasoning
    - Structured reasoning steps
    - Reasoning event callbacks
    - Step-by-step debugging
    """

    # Reasoning strategies
    REASONING_STRATEGIES = {
        "chain_of_thought": "Break down problem step by step",
        "tree_of_thought": "Explore multiple reasoning paths",
        "react": "Reason-Act-Observe loop",
        "reflection": "Self-reflection and correction",
        "decomposition": "Break into sub-problems",
    }

    def __init__(
        self,
        strategy: str = "chain_of_thought",
        max_steps: int = 10,
        verbose: bool = False,
        **kwargs
    ):
        """
        Initialize reasoning module.

        Args:
            strategy: Reasoning strategy to use
            max_steps: Maximum reasoning steps
            verbose: Enable verbose logging
            **kwargs: Additional configuration
        """
        self.strategy = strategy
        self.max_steps = max_steps
        self.verbose = verbose
        self.config = kwargs

        self._steps: List[Dict[str, Any]] = []
        self._event_handlers: Dict[str, List[Callable]] = {}
        self._current_step = 0

    def add_step(
        self,
        thought: str,
        action: Optional[str] = None,
        observation: Optional[str] = None,
        **metadata
    ) -> "ReasoningModule":
        """
        Add a reasoning step.

        Args:
            thought: The reasoning thought
            action: Optional action taken
            observation: Optional observation from action
            **metadata: Additional step metadata

        Returns:
            self for chaining
        """
        step = {
            "step_number": len(self._steps) + 1,
            "thought": thought,
            "action": action,
            "observation": observation,
            **metadata
        }
        self._steps.append(step)

        # Trigger step event
        self._trigger_event("step_added", step)

        if self.verbose:
            logger.info(f"Reasoning step {step['step_number']}: {thought[:100]}...")

        return self

    def on(self, event_type: str, handler: Callable) -> "ReasoningModule":
        """
        Register an event handler.

        Args:
            event_type: Event type ('step_added', 'reasoning_complete', etc.)
            handler: Callback function

        Returns:
            self for chaining
        """
        if event_type not in self._event_handlers:
            self._event_handlers[event_type] = []
        self._event_handlers[event_type].append(handler)
        return self

    def _trigger_event(self, event_type: str, data: Any) -> None:
        """Trigger event handlers"""
        handlers = self._event_handlers.get(event_type, [])
        for handler in handlers:
            try:
                handler(data)
            except Exception as e:
                logger.warning(f"Event handler error: {e}")

    def get_reasoning_prompt(self) -> str:
        """
        Get prompt enhancement for reasoning.

        Returns:
            Prompt string to add to agent instructions
        """
        prompts = {
            "chain_of_thought": """
Think through this step by step:
1. First, understand what is being asked
2. Break down the problem into smaller parts
3. Consider each part carefully
4. Show your reasoning for each step
5. Arrive at a final answer

Use "Let me think about this..." to start your reasoning.
""",
            "tree_of_thought": """
Consider multiple approaches to this problem:
1. Identify at least 2-3 different ways to approach this
2. Briefly evaluate each approach
3. Choose the most promising approach
4. Execute that approach step by step
5. Verify your answer

Start with "Let me consider different approaches..."
""",
            "react": """
Use the ReAct framework:
1. Thought: What do I need to figure out?
2. Action: What action should I take?
3. Observation: What did I learn from that action?
4. Repeat until you have enough information
5. Final Answer: Based on my reasoning...

Format your response with clear Thought/Action/Observation sections.
""",
            "reflection": """
Think carefully and reflect on your reasoning:
1. Initial thought: What's my first instinct?
2. Critical review: What could be wrong with this?
3. Alternative view: Is there another way to see this?
4. Refined answer: Based on reflection...
5. Confidence check: How confident am I?

Be willing to change your mind if you find errors.
""",
            "decomposition": """
Break this problem into sub-problems:
1. Identify the main components of the problem
2. List each sub-problem that needs to be solved
3. Solve each sub-problem independently
4. Combine the solutions
5. Verify the combined solution works

Start with "Let me break this down..."
""",
        }

        return prompts.get(self.strategy, prompts["chain_of_thought"])

    def create_reasoning_tools(self) -> List[Any]:
        """
        Create reasoning tools for the agent.

        Returns:
            List of reasoning tools
        """
        tools = []

        try:
            from agno.tools.reasoning import ReasoningTools
            tools.append(ReasoningTools())
        except ImportError:
            logger.debug("Agno ReasoningTools not available")

        return tools

    def get_steps(self) -> List[Dict[str, Any]]:
        """Get all reasoning steps"""
        return self._steps.copy()

    def get_last_step(self) -> Optional[Dict[str, Any]]:
        """Get the last reasoning step"""
        return self._steps[-1] if self._steps else None

    def clear_steps(self) -> None:
        """Clear all reasoning steps"""
        self._steps = []
        self._current_step = 0

    def to_agent_config(self) -> Dict[str, Any]:
        """
        Convert reasoning module to agent configuration.

        Returns:
            Dict with reasoning configuration
        """
        config = {
            "reasoning_prompt": self.get_reasoning_prompt(),
            "max_reasoning_steps": self.max_steps,
        }

        tools = self.create_reasoning_tools()
        if tools:
            config["reasoning_tools"] = tools

        return config

    def enhance_instructions(self, base_instructions: str) -> str:
        """
        Enhance agent instructions with reasoning prompt.

        Args:
            base_instructions: Original agent instructions

        Returns:
            Enhanced instructions
        """
        reasoning_prompt = self.get_reasoning_prompt()
        return f"{base_instructions}\n\n## Reasoning Approach\n{reasoning_prompt}"

    def format_steps_as_context(self) -> str:
        """
        Format reasoning steps as context string.

        Returns:
            Formatted string of reasoning steps
        """
        if not self._steps:
            return ""

        lines = ["## Previous Reasoning Steps\n"]
        for step in self._steps:
            lines.append(f"### Step {step['step_number']}")
            lines.append(f"**Thought:** {step['thought']}")
            if step.get("action"):
                lines.append(f"**Action:** {step['action']}")
            if step.get("observation"):
                lines.append(f"**Observation:** {step['observation']}")
            lines.append("")

        return "\n".join(lines)

    @classmethod
    def from_config(cls, config: Dict[str, Any]) -> "ReasoningModule":
        """
        Create ReasoningModule from configuration.

        Args:
            config: Reasoning configuration

        Returns:
            ReasoningModule instance
        """
        return cls(
            strategy=config.get("strategy", "chain_of_thought"),
            max_steps=config.get("max_steps", 10),
            verbose=config.get("verbose", False),
            **config.get("options", {})
        )

    @classmethod
    def list_strategies(cls) -> Dict[str, str]:
        """List available reasoning strategies"""
        return cls.REASONING_STRATEGIES.copy()


class ReasoningTracker:
    """
    Tracks and analyzes reasoning across multiple interactions.

    Useful for:
    - Debugging agent reasoning
    - Analyzing decision patterns
    - Improving prompts
    """

    def __init__(self):
        self.sessions: Dict[str, List[Dict[str, Any]]] = {}

    def start_session(self, session_id: str) -> None:
        """Start a new reasoning session"""
        self.sessions[session_id] = []

    def add_reasoning(
        self,
        session_id: str,
        query: str,
        reasoning_steps: List[Dict[str, Any]],
        final_answer: str
    ) -> None:
        """Add reasoning to a session"""
        if session_id not in self.sessions:
            self.start_session(session_id)

        self.sessions[session_id].append({
            "query": query,
            "steps": reasoning_steps,
            "final_answer": final_answer,
            "step_count": len(reasoning_steps),
        })

    def get_session_summary(self, session_id: str) -> Dict[str, Any]:
        """Get summary of a reasoning session"""
        if session_id not in self.sessions:
            return {}

        entries = self.sessions[session_id]
        total_steps = sum(e["step_count"] for e in entries)

        return {
            "session_id": session_id,
            "total_queries": len(entries),
            "total_reasoning_steps": total_steps,
            "avg_steps_per_query": total_steps / len(entries) if entries else 0,
            "entries": entries,
        }

    def clear_session(self, session_id: str) -> None:
        """Clear a reasoning session"""
        if session_id in self.sessions:
            del self.sessions[session_id]


class ReasoningBuilder:
    """
    Fluent builder for creating reasoning modules.

    Example:
        reasoning = (ReasoningBuilder()
            .with_strategy("react")
            .with_max_steps(15)
            .verbose()
            .build())
    """

    def __init__(self):
        """Initialize reasoning builder"""
        self._strategy = "chain_of_thought"
        self._max_steps = 10
        self._verbose = False
        self._config = {}

    def with_strategy(self, strategy: str) -> "ReasoningBuilder":
        """Set reasoning strategy"""
        self._strategy = strategy
        return self

    def with_max_steps(self, max_steps: int) -> "ReasoningBuilder":
        """Set maximum steps"""
        self._max_steps = max_steps
        return self

    def verbose(self, enabled: bool = True) -> "ReasoningBuilder":
        """Enable verbose mode"""
        self._verbose = enabled
        return self

    def with_config(self, **config) -> "ReasoningBuilder":
        """Add additional configuration"""
        self._config.update(config)
        return self

    def build(self) -> ReasoningModule:
        """Build and return the reasoning module"""
        return ReasoningModule(
            strategy=self._strategy,
            max_steps=self._max_steps,
            verbose=self._verbose,
            **self._config
        )
