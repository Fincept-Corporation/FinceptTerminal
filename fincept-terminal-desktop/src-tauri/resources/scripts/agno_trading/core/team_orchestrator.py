"""
Team Orchestrator

Multi-model orchestration inspired by Alpha Arena.
Runs multiple LLM agents in parallel, tracks performance, aggregates decisions.
"""

from typing import List, Dict, Any, Optional, Callable
from datetime import datetime
import asyncio
import json
from collections import defaultdict

try:
    from agno import Agent
    AGNO_AVAILABLE = True
except ImportError:
    AGNO_AVAILABLE = False
    Agent = None


class ModelPerformance:
    """Track performance metrics for each model"""

    def __init__(self, model_name: str):
        self.model_name = model_name
        self.total_trades = 0
        self.winning_trades = 0
        self.losing_trades = 0
        self.total_pnl = 0.0
        self.daily_pnl = 0.0
        self.current_positions = []
        self.decisions = []
        self.start_time = datetime.now()

    def add_decision(self, decision: Dict[str, Any]):
        """Log a model decision"""
        self.decisions.append({
            **decision,
            "timestamp": datetime.now().isoformat(),
            "model": self.model_name
        })

    def update_trade(self, pnl: float, is_win: bool):
        """Update trade statistics"""
        self.total_trades += 1
        self.total_pnl += pnl
        self.daily_pnl += pnl
        if is_win:
            self.winning_trades += 1
        else:
            self.losing_trades += 1

    def get_metrics(self) -> Dict[str, Any]:
        """Get current performance metrics"""
        win_rate = (self.winning_trades / self.total_trades * 100) if self.total_trades > 0 else 0
        return {
            "model": self.model_name,
            "total_trades": self.total_trades,
            "winning_trades": self.winning_trades,
            "losing_trades": self.losing_trades,
            "win_rate": win_rate,
            "total_pnl": self.total_pnl,
            "daily_pnl": self.daily_pnl,
            "positions": len(self.current_positions),
            "last_decision": self.decisions[-1] if self.decisions else None
        }


class TeamOrchestrator:
    """
    Multi-model orchestration for trading (Alpha Arena style)

    Features:
    - Run multiple models simultaneously
    - Track each model's performance
    - Stream real-time decisions
    - Aggregate signals with voting/consensus
    - Leaderboard & competition mode
    """

    def __init__(self):
        """Initialize team orchestrator"""
        self.teams: Dict[str, Dict] = {}
        self.model_performances: Dict[str, ModelPerformance] = {}
        self.decision_callbacks: List[Callable] = []
        self.created_at = datetime.now()

    def register_decision_callback(self, callback: Callable):
        """Register callback for real-time decision streaming"""
        self.decision_callbacks.append(callback)

    def _notify_decision(self, model: str, decision: Dict[str, Any]):
        """Notify all callbacks of new decision"""
        for callback in self.decision_callbacks:
            try:
                callback(model, decision)
            except Exception as e:
                print(f"[Orchestrator] Callback error: {e}")

    def create_competition(
        self,
        name: str,
        agents: List[Dict[str, Any]],
        task_type: str = "trading",
        mode: str = "parallel"
    ) -> str:
        """
        Create a multi-model competition

        Args:
            name: Competition name
            agents: List of agent configs [{"model": "openai:gpt-4", "agent": agent_instance}]
            task_type: Type of task (trading, analysis, etc.)
            mode: Execution mode (parallel, sequential, voting)

        Returns:
            team_id: Competition identifier
        """
        if not AGNO_AVAILABLE:
            raise ImportError("Agno framework required for creating competitions")

        team_id = f"comp_{name}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"

        # Initialize performance tracking for each model
        for agent_config in agents:
            model_name = agent_config["model"]
            if model_name not in self.model_performances:
                self.model_performances[model_name] = ModelPerformance(model_name)

        self.teams[team_id] = {
            "name": name,
            "agents": agents,
            "task_type": task_type,
            "mode": mode,
            "created_at": datetime.now().isoformat(),
            "is_running": False
        }

        return team_id

    async def run_parallel(
        self,
        team_id: str,
        prompt: str,
        streaming: bool = True
    ) -> Dict[str, Any]:
        """
        Run all models in parallel (Alpha Arena style)

        Args:
            team_id: Team identifier
            prompt: Prompt/task for agents
            streaming: Enable real-time decision streaming

        Returns:
            Aggregated results from all models
        """
        if team_id not in self.teams:
            raise ValueError(f"Team not found: {team_id}")

        team = self.teams[team_id]
        team["is_running"] = True

        # Run all agents concurrently
        tasks = []
        for agent_config in team["agents"]:
            tasks.append(self._run_single_agent(
                agent_config["model"],
                agent_config["agent"],
                prompt,
                streaming
            ))

        results = await asyncio.gather(*tasks, return_exceptions=True)

        # Aggregate results
        successful_results = []
        for i, result in enumerate(results):
            if isinstance(result, Exception):
                print(f"[Orchestrator] Agent {team['agents'][i]['model']} failed: {result}")
            else:
                successful_results.append(result)

        team["is_running"] = False

        return {
            "team_id": team_id,
            "results": successful_results,
            "consensus": self._calculate_consensus(successful_results),
            "leaderboard": self.get_leaderboard(),
            "timestamp": datetime.now().isoformat()
        }

    async def _run_single_agent(
        self,
        model_name: str,
        agent: Any,
        prompt: str,
        streaming: bool
    ) -> Dict[str, Any]:
        """Run a single agent and track its decision"""
        try:
            # Execute agent
            response = agent.run(prompt)

            # Extract decision
            decision = self._parse_agent_response(response)

            # Track performance
            perf = self.model_performances[model_name]
            perf.add_decision(decision)

            # Stream decision to UI
            if streaming:
                self._notify_decision(model_name, decision)

            return {
                "model": model_name,
                "decision": decision,
                "response": self._extract_response_content(response),
                "timestamp": datetime.now().isoformat()
            }

        except Exception as e:
            return {
                "model": model_name,
                "error": str(e),
                "timestamp": datetime.now().isoformat()
            }

    def _parse_agent_response(self, response: Any) -> Dict[str, Any]:
        """Parse agent response into structured decision"""
        content = self._extract_response_content(response)

        # Try to extract JSON decision
        try:
            import re
            json_match = re.search(r'\{[^}]+\}', content, re.DOTALL)
            if json_match:
                return json.loads(json_match.group())
        except:
            pass

        # Return text decision
        return {
            "type": "analysis",
            "content": content,
            "action": "hold"
        }

    def _extract_response_content(self, response: Any) -> str:
        """Extract text content from agent response"""
        if hasattr(response, 'content'):
            return response.content
        elif isinstance(response, dict) and 'content' in response:
            return response['content']
        elif isinstance(response, str):
            return response
        return str(response)

    def _calculate_consensus(self, results: List[Dict]) -> Dict[str, Any]:
        """Calculate consensus from multiple model decisions"""
        if not results:
            return {"action": "hold", "confidence": 0.0, "agreement": 0}

        actions = defaultdict(int)
        for result in results:
            decision = result.get("decision", {})
            action = decision.get("action", "hold")
            actions[action] += 1

        # Majority vote
        consensus_action = max(actions.items(), key=lambda x: x[1])
        total = len(results)

        return {
            "action": consensus_action[0],
            "votes": dict(actions),
            "confidence": consensus_action[1] / total,
            "agreement": consensus_action[1],
            "total_models": total
        }

    def get_leaderboard(self) -> List[Dict[str, Any]]:
        """Get performance leaderboard of all models"""
        leaderboard = []
        for model_name, perf in self.model_performances.items():
            leaderboard.append(perf.get_metrics())

        # Sort by total PnL
        leaderboard.sort(key=lambda x: x["total_pnl"], reverse=True)

        return leaderboard

    def get_recent_decisions(self, limit: int = 50) -> List[Dict[str, Any]]:
        """Get recent decisions from all models"""
        all_decisions = []
        for perf in self.model_performances.values():
            all_decisions.extend(perf.decisions)

        # Sort by timestamp descending
        all_decisions.sort(key=lambda x: x["timestamp"], reverse=True)

        return all_decisions[:limit]

    def update_trade_result(self, model: str, pnl: float):
        """Update trade result for a model"""
        if model in self.model_performances:
            is_win = pnl > 0
            self.model_performances[model].update_trade(pnl, is_win)

    def reset_daily_stats(self):
        """Reset daily statistics for all models"""
        for perf in self.model_performances.values():
            perf.daily_pnl = 0.0
