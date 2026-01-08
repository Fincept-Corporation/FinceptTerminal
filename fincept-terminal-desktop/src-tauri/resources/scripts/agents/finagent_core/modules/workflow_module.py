"""
Workflow Module - Complex task orchestration

Provides:
- Sequential and parallel step execution
- Conditional branching
- Loop support
- Step dependencies
- Agent workflow integration
"""

from typing import Dict, Any, Optional, List, Callable, Union
import logging

logger = logging.getLogger(__name__)


class WorkflowModule:
    """
    Workflow orchestration for complex multi-step tasks.

    Supports:
    - Sequential steps
    - Parallel execution
    - Conditional branching
    - Loops
    - Agent integration
    """

    def __init__(
        self,
        name: str = "Workflow",
        description: Optional[str] = None,
        **kwargs
    ):
        """
        Initialize workflow module.

        Args:
            name: Workflow name
            description: Workflow description
            **kwargs: Additional configuration
        """
        self.name = name
        self.description = description
        self.config = kwargs

        self._steps: List[Dict[str, Any]] = []
        self._workflow = None

    def add_step(
        self,
        name: str,
        agent: Optional[Any] = None,
        function: Optional[Callable] = None,
        prompt: Optional[str] = None,
        **kwargs
    ) -> "WorkflowModule":
        """
        Add a step to the workflow.

        Args:
            name: Step name
            agent: Agno Agent for this step
            function: Python function to execute
            prompt: Prompt template for agent
            **kwargs: Additional step configuration

        Returns:
            self for chaining
        """
        step = {
            "type": "step",
            "name": name,
            "agent": agent,
            "function": function,
            "prompt": prompt,
            **kwargs
        }
        self._steps.append(step)
        return self

    def add_parallel(
        self,
        name: str,
        steps: List[Dict[str, Any]],
        **kwargs
    ) -> "WorkflowModule":
        """
        Add parallel execution steps.

        Args:
            name: Parallel group name
            steps: List of steps to run in parallel
            **kwargs: Additional configuration

        Returns:
            self for chaining
        """
        parallel_step = {
            "type": "parallel",
            "name": name,
            "steps": steps,
            **kwargs
        }
        self._steps.append(parallel_step)
        return self

    def add_condition(
        self,
        name: str,
        condition: Callable[[Any], bool],
        if_true: Dict[str, Any],
        if_false: Optional[Dict[str, Any]] = None,
        **kwargs
    ) -> "WorkflowModule":
        """
        Add conditional branching.

        Args:
            name: Condition name
            condition: Function that returns True/False
            if_true: Step to execute if condition is True
            if_false: Step to execute if condition is False
            **kwargs: Additional configuration

        Returns:
            self for chaining
        """
        condition_step = {
            "type": "condition",
            "name": name,
            "condition": condition,
            "if_true": if_true,
            "if_false": if_false,
            **kwargs
        }
        self._steps.append(condition_step)
        return self

    def add_loop(
        self,
        name: str,
        steps: List[Dict[str, Any]],
        condition: Optional[Callable[[Any], bool]] = None,
        max_iterations: int = 10,
        **kwargs
    ) -> "WorkflowModule":
        """
        Add loop execution.

        Args:
            name: Loop name
            steps: Steps to repeat
            condition: Continue condition (loop while True)
            max_iterations: Maximum iterations
            **kwargs: Additional configuration

        Returns:
            self for chaining
        """
        loop_step = {
            "type": "loop",
            "name": name,
            "steps": steps,
            "condition": condition,
            "max_iterations": max_iterations,
            **kwargs
        }
        self._steps.append(loop_step)
        return self

    def add_router(
        self,
        name: str,
        routes: Dict[str, Dict[str, Any]],
        router_function: Callable[[Any], str],
        **kwargs
    ) -> "WorkflowModule":
        """
        Add router step for dynamic routing.

        Args:
            name: Router name
            routes: Dict of route_key -> step configuration
            router_function: Function that returns route key
            **kwargs: Additional configuration

        Returns:
            self for chaining
        """
        router_step = {
            "type": "router",
            "name": name,
            "routes": routes,
            "router_function": router_function,
            **kwargs
        }
        self._steps.append(router_step)
        return self

    def build(self) -> Any:
        """
        Build the Agno Workflow.

        Returns:
            Agno Workflow instance
        """
        if self._workflow:
            return self._workflow

        if not self._steps:
            raise ValueError("Cannot build workflow without steps")

        try:
            from agno.workflow import Workflow, Step, Parallel, Condition, Loop, Router

            # Convert steps to Agno format
            agno_steps = self._convert_steps_to_agno(self._steps)

            self._workflow = Workflow(
                name=self.name,
                description=self.description,
                steps=agno_steps
            )

            logger.debug(f"Built workflow '{self.name}' with {len(self._steps)} steps")
            return self._workflow

        except ImportError as e:
            logger.warning(f"Agno Workflow not available: {e}")
            # Return a simple executor fallback
            return self._create_fallback_executor()
        except Exception as e:
            raise RuntimeError(f"Failed to build workflow: {e}")

    def _convert_steps_to_agno(self, steps: List[Dict[str, Any]]) -> List[Any]:
        """Convert internal step format to Agno steps"""
        from agno.workflow import Step, Parallel, Condition, Loop, Router

        agno_steps = []

        for step_config in steps:
            step_type = step_config.get("type", "step")

            if step_type == "step":
                agno_step = self._create_step(step_config)
            elif step_type == "parallel":
                agno_step = self._create_parallel(step_config)
            elif step_type == "condition":
                agno_step = self._create_condition(step_config)
            elif step_type == "loop":
                agno_step = self._create_loop(step_config)
            elif step_type == "router":
                agno_step = self._create_router(step_config)
            else:
                logger.warning(f"Unknown step type: {step_type}")
                continue

            if agno_step:
                agno_steps.append(agno_step)

        return agno_steps

    def _create_step(self, config: Dict[str, Any]) -> Any:
        """Create an Agno Step"""
        from agno.workflow import Step

        step_kwargs = {"name": config["name"]}

        if config.get("agent"):
            step_kwargs["agent"] = config["agent"]
        if config.get("function"):
            step_kwargs["function"] = config["function"]
        if config.get("prompt"):
            step_kwargs["prompt"] = config["prompt"]

        return Step(**step_kwargs)

    def _create_parallel(self, config: Dict[str, Any]) -> Any:
        """Create an Agno Parallel step"""
        from agno.workflow import Parallel

        inner_steps = self._convert_steps_to_agno(config.get("steps", []))
        return Parallel(name=config["name"], steps=inner_steps)

    def _create_condition(self, config: Dict[str, Any]) -> Any:
        """Create an Agno Condition step"""
        from agno.workflow import Condition

        return Condition(
            name=config["name"],
            condition=config["condition"],
            if_true=self._create_step(config["if_true"]) if config.get("if_true") else None,
            if_false=self._create_step(config["if_false"]) if config.get("if_false") else None,
        )

    def _create_loop(self, config: Dict[str, Any]) -> Any:
        """Create an Agno Loop step"""
        from agno.workflow import Loop

        inner_steps = self._convert_steps_to_agno(config.get("steps", []))
        return Loop(
            name=config["name"],
            steps=inner_steps,
            condition=config.get("condition"),
            max_iterations=config.get("max_iterations", 10),
        )

    def _create_router(self, config: Dict[str, Any]) -> Any:
        """Create an Agno Router step"""
        from agno.workflow import Router

        routes = {}
        for key, step_config in config.get("routes", {}).items():
            routes[key] = self._create_step(step_config)

        return Router(
            name=config["name"],
            routes=routes,
            router=config["router_function"],
        )

    def _create_fallback_executor(self) -> "SimpleWorkflowExecutor":
        """Create a simple fallback executor if Agno Workflow is not available"""
        return SimpleWorkflowExecutor(self._steps)

    def run(self, input_data: Optional[Dict[str, Any]] = None, **kwargs) -> Any:
        """
        Run the workflow.

        Args:
            input_data: Initial input data
            **kwargs: Additional arguments

        Returns:
            Workflow result
        """
        workflow = self.build()

        run_kwargs = {}
        if input_data:
            run_kwargs["input"] = input_data
        run_kwargs.update(kwargs)

        return workflow.run(**run_kwargs)

    async def arun(self, input_data: Optional[Dict[str, Any]] = None, **kwargs) -> Any:
        """
        Run the workflow asynchronously.

        Args:
            input_data: Initial input data
            **kwargs: Additional arguments

        Returns:
            Workflow result
        """
        workflow = self.build()

        run_kwargs = {}
        if input_data:
            run_kwargs["input"] = input_data
        run_kwargs.update(kwargs)

        return await workflow.arun(**run_kwargs)

    def get_steps(self) -> List[Dict[str, Any]]:
        """Get list of workflow steps"""
        return self._steps.copy()

    @classmethod
    def from_config(cls, config: Dict[str, Any]) -> "WorkflowModule":
        """
        Create WorkflowModule from configuration.

        Args:
            config: Workflow configuration

        Returns:
            WorkflowModule instance
        """
        module = cls(
            name=config.get("name", "Workflow"),
            description=config.get("description"),
        )

        for step_config in config.get("steps", []):
            step_type = step_config.get("type", "step")

            if step_type == "step":
                module.add_step(**step_config)
            elif step_type == "parallel":
                module.add_parallel(**step_config)
            elif step_type == "condition":
                module.add_condition(**step_config)
            elif step_type == "loop":
                module.add_loop(**step_config)
            elif step_type == "router":
                module.add_router(**step_config)

        return module


class SimpleWorkflowExecutor:
    """
    Simple fallback workflow executor when Agno Workflow is not available.
    Executes steps sequentially.
    """

    def __init__(self, steps: List[Dict[str, Any]]):
        self.steps = steps

    def run(self, input: Optional[Dict[str, Any]] = None, **kwargs) -> Dict[str, Any]:
        """Execute workflow steps sequentially"""
        context = input or {}
        results = []

        for step in self.steps:
            step_type = step.get("type", "step")

            if step_type == "step":
                result = self._execute_step(step, context)
                results.append(result)
                context["last_result"] = result
            elif step_type == "parallel":
                # Execute sequentially as fallback
                for inner_step in step.get("steps", []):
                    result = self._execute_step(inner_step, context)
                    results.append(result)
            # Add other step types as needed

        return {
            "success": True,
            "results": results,
            "context": context
        }

    def _execute_step(self, step: Dict[str, Any], context: Dict[str, Any]) -> Any:
        """Execute a single step"""
        if step.get("function"):
            return step["function"](context)
        elif step.get("agent"):
            prompt = step.get("prompt", "")
            return step["agent"].run(prompt.format(**context))
        return None

    async def arun(self, input: Optional[Dict[str, Any]] = None, **kwargs) -> Dict[str, Any]:
        """Async execution (falls back to sync)"""
        return self.run(input=input, **kwargs)


class WorkflowBuilder:
    """
    Fluent builder for creating workflows.

    Example:
        workflow = (WorkflowBuilder("Data Pipeline")
            .add_step("fetch", function=fetch_data)
            .add_step("process", agent=processor_agent)
            .add_step("save", function=save_results)
            .build())
    """

    def __init__(self, name: str):
        """Initialize workflow builder"""
        self._module = WorkflowModule(name=name)

    def with_description(self, description: str) -> "WorkflowBuilder":
        """Set workflow description"""
        self._module.description = description
        return self

    def add_step(
        self,
        name: str,
        agent: Optional[Any] = None,
        function: Optional[Callable] = None,
        prompt: Optional[str] = None,
        **kwargs
    ) -> "WorkflowBuilder":
        """Add a step"""
        self._module.add_step(name, agent=agent, function=function, prompt=prompt, **kwargs)
        return self

    def add_parallel(self, name: str, steps: List[Dict[str, Any]]) -> "WorkflowBuilder":
        """Add parallel steps"""
        self._module.add_parallel(name, steps)
        return self

    def add_condition(
        self,
        name: str,
        condition: Callable,
        if_true: Dict,
        if_false: Optional[Dict] = None
    ) -> "WorkflowBuilder":
        """Add conditional branching"""
        self._module.add_condition(name, condition, if_true, if_false)
        return self

    def add_loop(
        self,
        name: str,
        steps: List[Dict],
        condition: Optional[Callable] = None,
        max_iterations: int = 10
    ) -> "WorkflowBuilder":
        """Add a loop"""
        self._module.add_loop(name, steps, condition, max_iterations)
        return self

    def build(self) -> Any:
        """Build and return the workflow"""
        return self._module.build()

    def get_module(self) -> WorkflowModule:
        """Get the underlying WorkflowModule"""
        return self._module
