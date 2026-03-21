"""
Agent Manager

This module manages the lifecycle of trading agents:
- Creating and destroying agents
- Tracking active agents
- Managing agent resources
- Agent performance monitoring
"""

import json
from typing import Dict, List, Optional, Any
from datetime import datetime
from dataclasses import dataclass, asdict

from .base_agent import BaseAgent, AgentConfig


@dataclass
class AgentMetrics:
    """Performance metrics for an agent"""
    total_runs: int = 0
    successful_runs: int = 0
    failed_runs: int = 0
    total_tokens_used: int = 0
    total_cost: float = 0.0
    average_response_time: float = 0.0
    last_run_at: Optional[str] = None


class AgentManager:
    """
    Manages lifecycle and monitoring of trading agents

    Features:
    - Agent creation and destruction
    - Agent registry and lookup
    - Performance metrics tracking
    - Resource management
    - Agent health monitoring
    """

    def __init__(self):
        """Initialize the agent manager"""
        self.agents: Dict[str, BaseAgent] = {}
        self.metrics: Dict[str, AgentMetrics] = {}
        self.created_at = datetime.now()

    def create_agent(self, config: AgentConfig) -> str:
        """
        Create a new trading agent

        Args:
            config: Agent configuration

        Returns:
            agent_id: Unique identifier for the created agent

        Raises:
            ValueError: If agent creation fails
        """
        try:
            # Create agent instance
            agent = BaseAgent(config)

            # Register agent
            agent_id = agent.agent_id
            self.agents[agent_id] = agent
            self.metrics[agent_id] = AgentMetrics()

            return agent_id

        except Exception as e:
            raise ValueError(f"Failed to create agent: {str(e)}")

    def get_agent(self, agent_id: str) -> Optional[BaseAgent]:
        """
        Get an agent by ID

        Args:
            agent_id: Agent identifier

        Returns:
            Agent instance or None if not found
        """
        return self.agents.get(agent_id)

    def list_agents(self) -> List[Dict[str, Any]]:
        """
        List all active agents

        Returns:
            List of agent information dictionaries
        """
        return [
            {
                **agent.to_dict(),
                "metrics": asdict(self.metrics.get(agent_id, AgentMetrics()))
            }
            for agent_id, agent in self.agents.items()
        ]

    def destroy_agent(self, agent_id: str) -> bool:
        """
        Destroy an agent and clean up resources

        Args:
            agent_id: Agent identifier

        Returns:
            True if agent was destroyed, False if not found
        """
        if agent_id in self.agents:
            # Clean up agent resources
            agent = self.agents[agent_id]
            # Clear all sessions
            for session_id in list(agent.sessions.keys()):
                agent.clear_session(session_id)

            # Remove from registry
            del self.agents[agent_id]

            # Keep metrics for historical tracking
            # but mark as destroyed
            if agent_id in self.metrics:
                self.metrics[agent_id].last_run_at = datetime.now().isoformat()

            return True

        return False

    def run_agent(
        self,
        agent_id: str,
        prompt: str,
        session_id: Optional[str] = None,
        stream: bool = False,
        **kwargs
    ) -> Dict[str, Any]:
        """
        Run an agent with a prompt and track metrics

        Args:
            agent_id: Agent identifier
            prompt: User prompt
            session_id: Optional session ID
            stream: Whether to stream response
            **kwargs: Additional parameters

        Returns:
            Response dictionary with content and metadata

        Raises:
            ValueError: If agent not found or execution fails
        """
        # Get agent
        agent = self.get_agent(agent_id)
        if not agent:
            raise ValueError(f"Agent not found: {agent_id}")

        # Track metrics
        metrics = self.metrics.get(agent_id, AgentMetrics())
        start_time = datetime.now()

        try:
            # Run agent
            response = agent.run(
                prompt=prompt,
                session_id=session_id,
                stream=stream,
                **kwargs
            )

            # Calculate response time
            end_time = datetime.now()
            response_time = (end_time - start_time).total_seconds()

            # Update metrics
            metrics.total_runs += 1
            metrics.successful_runs += 1
            metrics.last_run_at = end_time.isoformat()

            # Update average response time
            if metrics.average_response_time == 0:
                metrics.average_response_time = response_time
            else:
                metrics.average_response_time = (
                    (metrics.average_response_time * (metrics.total_runs - 1) + response_time) /
                    metrics.total_runs
                )

            # Extract token usage and cost if available
            if hasattr(response, 'metrics'):
                if hasattr(response.metrics, 'input_tokens'):
                    metrics.total_tokens_used += response.metrics.input_tokens
                if hasattr(response.metrics, 'output_tokens'):
                    metrics.total_tokens_used += response.metrics.output_tokens

            self.metrics[agent_id] = metrics

            # Format response
            result = {
                "agent_id": agent_id,
                "session_id": session_id,
                "content": agent._extract_response_content(response),
                "response_time": response_time,
                "timestamp": end_time.isoformat(),
                "stream": stream,
            }

            # Add metrics if available
            if hasattr(response, 'metrics'):
                result["metrics"] = {
                    "input_tokens": getattr(response.metrics, 'input_tokens', 0),
                    "output_tokens": getattr(response.metrics, 'output_tokens', 0),
                    "total_tokens": getattr(response.metrics, 'total_tokens', 0),
                }

            return result

        except Exception as e:
            # Update failure metrics
            metrics.total_runs += 1
            metrics.failed_runs += 1
            metrics.last_run_at = datetime.now().isoformat()
            self.metrics[agent_id] = metrics

            raise ValueError(f"Agent execution failed: {str(e)}")

    def get_agent_metrics(self, agent_id: str) -> Optional[Dict[str, Any]]:
        """
        Get performance metrics for an agent

        Args:
            agent_id: Agent identifier

        Returns:
            Metrics dictionary or None if not found
        """
        metrics = self.metrics.get(agent_id)
        if metrics:
            return asdict(metrics)
        return None

    def get_session_history(
        self,
        agent_id: str,
        session_id: str
    ) -> List[Dict[str, Any]]:
        """
        Get conversation history for an agent session

        Args:
            agent_id: Agent identifier
            session_id: Session identifier

        Returns:
            List of conversation messages
        """
        agent = self.get_agent(agent_id)
        if agent:
            return agent.get_session_history(session_id)
        return []

    def clear_session(self, agent_id: str, session_id: str) -> bool:
        """
        Clear a specific agent session

        Args:
            agent_id: Agent identifier
            session_id: Session identifier

        Returns:
            True if session was cleared, False if not found
        """
        agent = self.get_agent(agent_id)
        if agent:
            agent.clear_session(session_id)
            return True
        return False

    def get_manager_stats(self) -> Dict[str, Any]:
        """
        Get overall manager statistics

        Returns:
            Statistics dictionary
        """
        total_agents = len(self.agents)
        total_runs = sum(m.total_runs for m in self.metrics.values())
        total_successful = sum(m.successful_runs for m in self.metrics.values())
        total_failed = sum(m.failed_runs for m in self.metrics.values())
        total_tokens = sum(m.total_tokens_used for m in self.metrics.values())
        total_cost = sum(m.total_cost for m in self.metrics.values())

        return {
            "manager_uptime": (datetime.now() - self.created_at).total_seconds(),
            "total_agents": total_agents,
            "active_agents": total_agents,
            "total_runs": total_runs,
            "successful_runs": total_successful,
            "failed_runs": total_failed,
            "success_rate": (total_successful / total_runs * 100) if total_runs > 0 else 0,
            "total_tokens_used": total_tokens,
            "total_cost": total_cost,
        }

    def health_check(self) -> Dict[str, Any]:
        """
        Perform health check on all agents

        Returns:
            Health status dictionary
        """
        healthy_agents = []
        unhealthy_agents = []

        for agent_id, agent in self.agents.items():
            metrics = self.metrics.get(agent_id, AgentMetrics())

            # Basic health criteria
            is_healthy = True
            health_issues = []

            # Check failure rate
            if metrics.total_runs > 0:
                failure_rate = metrics.failed_runs / metrics.total_runs
                if failure_rate > 0.5:  # More than 50% failures
                    is_healthy = False
                    health_issues.append(f"High failure rate: {failure_rate:.1%}")

            # Check if agent has been used recently
            if metrics.last_run_at:
                # Agent has been used, all good
                pass
            elif (datetime.now() - agent.created_at).total_seconds() > 3600:
                # Agent created more than 1 hour ago but never used
                health_issues.append("Agent never used")

            agent_health = {
                "agent_id": agent_id,
                "name": agent.config.name,
                "is_healthy": is_healthy,
                "issues": health_issues,
                "metrics": asdict(metrics)
            }

            if is_healthy:
                healthy_agents.append(agent_health)
            else:
                unhealthy_agents.append(agent_health)

        return {
            "timestamp": datetime.now().isoformat(),
            "total_agents": len(self.agents),
            "healthy_count": len(healthy_agents),
            "unhealthy_count": len(unhealthy_agents),
            "healthy_agents": healthy_agents,
            "unhealthy_agents": unhealthy_agents,
        }

    def __repr__(self) -> str:
        return f"<AgentManager(agents={len(self.agents)}, uptime={(datetime.now() - self.created_at).total_seconds():.0f}s)>"


# Global agent manager instance
_agent_manager = None


def get_agent_manager() -> AgentManager:
    """Get the global agent manager instance"""
    global _agent_manager
    if _agent_manager is None:
        _agent_manager = AgentManager()
    return _agent_manager
