"""
FinAgent Core Main Entry Point

Unified entry point for all agent operations via Tauri.
Handles JSON payload dispatch to appropriate modules.
"""

import sys
import json
from pathlib import Path
from typing import Dict, Any

# Add parent directory to path
parent_dir = str(Path(__file__).parent.parent)
if parent_dir not in sys.path:
    sys.path.insert(0, parent_dir)


def main(args=None):
    """
    Main entry point - accepts single JSON payload from Rust/Tauri.

    Payload format:
    {
        "action": "<action_name>",
        "api_keys": {...},
        "params": {...}
    }

    Actions:
    - Core agent: run, run_team, run_workflow, run_structured
    - Dynamic loading: discover_agents, list_agents, create_agent
    - SuperAgent: route_query, execute_query
    - Execution planner: create_plan, execute_plan, get_plan_status
    - Paper trading: execute_trade, get_portfolio_value, get_positions_summary
    - System: system_info, list_tools, list_models
    """
    if args is None:
        args = sys.argv[1:]

    try:
        if len(args) == 0:
            return json.dumps({"success": False, "error": "No payload provided"})

        payload = json.loads(args[0])
        action = payload.get("action")
        api_keys = payload.get("api_keys", {})
        params = payload.get("params", {})
        config = payload.get("config", {})

        if not action:
            return json.dumps({"success": False, "error": "Missing 'action' in payload"})

        result = dispatch_action(action, api_keys, params, config)
        return json.dumps(result)

    except json.JSONDecodeError as e:
        return json.dumps({"success": False, "error": f"Invalid JSON: {str(e)}"})
    except Exception as e:
        return json.dumps({"success": False, "error": str(e)})


def dispatch_action(
    action: str,
    api_keys: Dict[str, str],
    params: Dict[str, Any],
    config: Dict[str, Any]
) -> Dict[str, Any]:
    """Dispatch action to appropriate handler"""

    # =========================================================================
    # Core Agent Actions
    # =========================================================================

    if action == "run":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys, user_id=params.get("user_id"))
        query = params.get("query")
        if not query:
            return {"success": False, "error": "Missing 'query' in params"}
        response = agent.run(query, config, params.get("session_id"))
        return {"success": True, "response": agent.get_response_content(response)}

    if action == "run_team":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys)
        query = params.get("query")
        team_config = params.get("team_config", config)
        if not query:
            return {"success": False, "error": "Missing 'query' in params"}
        response = agent.run_team(query, team_config, params.get("session_id"))
        return {"success": True, "response": agent.get_response_content(response)}

    if action == "run_workflow":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys)
        workflow_config = params.get("workflow_config", config)
        input_data = params.get("input_data", {})
        result = agent.run_workflow(workflow_config, input_data)
        return {"success": True, "result": result if isinstance(result, dict) else str(result)}

    if action == "run_structured":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys)
        query = params.get("query")
        output_model = params.get("output_model")
        if not query or not output_model:
            return {"success": False, "error": "Missing 'query' or 'output_model'"}
        return agent.run_with_output_model(query, config, output_model, params.get("session_id"))

    # =========================================================================
    # Dynamic Agent Loading
    # =========================================================================

    if action == "discover_agents":
        from finagent_core.agent_loader import discover_agents
        agents = discover_agents()
        return {"success": True, "agents": agents, "count": len(agents)}

    if action == "list_agents":
        from finagent_core.agent_loader import list_agents, list_categories
        category = params.get("category")
        agents = list_agents(category)
        return {
            "success": True,
            "agents": agents,
            "count": len(agents),
            "categories": list_categories()
        }

    if action == "create_agent":
        from finagent_core.agent_loader import create_agent
        agent_id = params.get("agent_id")
        if not agent_id:
            return {"success": False, "error": "Missing 'agent_id'"}
        try:
            agent = create_agent(agent_id, api_keys, config)
            return {"success": True, "agent_id": agent_id, "created": True}
        except Exception as e:
            return {"success": False, "error": str(e)}

    # =========================================================================
    # SuperAgent Routing
    # =========================================================================

    if action == "route_query":
        from finagent_core.super_agent import route_query
        query = params.get("query")
        if not query:
            return {"success": False, "error": "Missing 'query'"}
        return route_query(query, api_keys)

    if action == "execute_query":
        from finagent_core.super_agent import execute_query
        query = params.get("query")
        if not query:
            return {"success": False, "error": "Missing 'query'"}
        return execute_query(query, api_keys, params.get("session_id"))

    if action == "execute_multi_query":
        from finagent_core.super_agent import SuperAgent
        query = params.get("query")
        if not query:
            return {"success": False, "error": "Missing 'query'"}
        agent = SuperAgent(api_keys=api_keys)
        return agent.execute_multi(query, params.get("session_id"), params.get("aggregate", True))

    # =========================================================================
    # Execution Planner
    # =========================================================================

    if action == "create_stock_plan":
        from finagent_core.execution_planner import create_stock_analysis_plan
        symbol = params.get("symbol")
        if not symbol:
            return {"success": False, "error": "Missing 'symbol'"}
        plan = create_stock_analysis_plan(symbol)
        return {"success": True, "plan": plan}

    if action == "create_portfolio_plan":
        from finagent_core.execution_planner import ExecutionPlanner
        portfolio_id = params.get("portfolio_id")
        if not portfolio_id:
            return {"success": False, "error": "Missing 'portfolio_id'"}
        plan = ExecutionPlanner.portfolio_rebalance_plan(portfolio_id)
        return {"success": True, "plan": plan.to_dict()}

    if action == "execute_plan":
        from finagent_core.execution_planner import execute_plan
        plan_dict = params.get("plan")
        if not plan_dict:
            return {"success": False, "error": "Missing 'plan'"}
        return execute_plan(plan_dict, api_keys)

    # =========================================================================
    # Paper Trading Bridge
    # =========================================================================

    if action == "paper_execute_trade":
        from finagent_core.paper_trading_bridge import execute_trade
        portfolio_id = params.get("portfolio_id")
        symbol = params.get("symbol")
        trade_action = params.get("action")
        quantity = params.get("quantity", 1)
        price = params.get("price")
        if not all([portfolio_id, symbol, trade_action, price]):
            return {"success": False, "error": "Missing required params: portfolio_id, symbol, action, price"}
        return execute_trade(portfolio_id, symbol, trade_action, quantity, price)

    if action == "paper_get_portfolio":
        from finagent_core.paper_trading_bridge import get_portfolio_value
        portfolio_id = params.get("portfolio_id")
        if not portfolio_id:
            return {"success": False, "error": "Missing 'portfolio_id'"}
        return get_portfolio_value(portfolio_id)

    if action == "paper_get_positions":
        from finagent_core.paper_trading_bridge import get_positions_summary
        portfolio_id = params.get("portfolio_id")
        if not portfolio_id:
            return {"success": False, "error": "Missing 'portfolio_id'"}
        return get_positions_summary(portfolio_id)

    # =========================================================================
    # Repository Operations
    # =========================================================================

    if action == "save_session":
        from finagent_core.repositories import RepositoryFactory, AgentSession
        repo = RepositoryFactory.get_session_repository()
        session = AgentSession(**params)
        session_id = repo.create(session)
        return {"success": True, "session_id": session_id}

    if action == "get_session":
        from finagent_core.repositories import RepositoryFactory
        repo = RepositoryFactory.get_session_repository()
        session = repo.get(params.get("session_id"))
        if session:
            return {"success": True, "session": {
                "id": session.id,
                "agent_id": session.agent_id,
                "messages": session.messages,
                "status": session.status
            }}
        return {"success": False, "error": "Session not found"}

    if action == "add_message":
        from finagent_core.repositories import RepositoryFactory
        repo = RepositoryFactory.get_session_repository()
        success = repo.add_message(
            params.get("session_id"),
            params.get("role"),
            params.get("content")
        )
        return {"success": success}

    if action == "save_memory":
        from finagent_core.repositories import RepositoryFactory, AgentMemory
        import uuid
        repo = RepositoryFactory.get_memory_repository()
        memory = AgentMemory(
            id=str(uuid.uuid4()),
            agent_id=params.get("agent_id", "default"),
            user_id=params.get("user_id"),
            memory_type=params.get("type", "fact"),
            content=params.get("content"),
            metadata=params.get("metadata", {}),
            importance=params.get("importance", 0.5)
        )
        memory_id = repo.create(memory)
        return {"success": True, "memory_id": memory_id}

    if action == "search_memories":
        from finagent_core.repositories import RepositoryFactory
        repo = RepositoryFactory.get_memory_repository()
        memories = repo.search(
            params.get("query", ""),
            params.get("agent_id"),
            params.get("limit", 10)
        )
        return {
            "success": True,
            "memories": [
                {"id": m.id, "content": m.content, "type": m.memory_type, "importance": m.importance}
                for m in memories
            ]
        }

    if action == "save_trade_decision":
        from finagent_core.repositories import RepositoryFactory, TradeDecision
        import uuid
        from datetime import datetime
        repo = RepositoryFactory.get_trade_decision_repository()
        decision = TradeDecision(
            id=str(uuid.uuid4()),
            competition_id=params.get("competition_id"),
            model_name=params.get("model_name"),
            cycle_number=params.get("cycle_number", 0),
            symbol=params.get("symbol"),
            action=params.get("action"),
            quantity=params.get("quantity", 0),
            price=params.get("price"),
            reasoning=params.get("reasoning", ""),
            confidence=params.get("confidence", 0.5),
            timestamp=datetime.utcnow().isoformat()
        )
        decision_id = repo.create(decision)
        return {"success": True, "decision_id": decision_id}

    if action == "get_decisions":
        from finagent_core.repositories import RepositoryFactory
        repo = RepositoryFactory.get_trade_decision_repository()
        decisions = repo.list(
            competition_id=params.get("competition_id"),
            model_name=params.get("model_name"),
            cycle_number=params.get("cycle_number"),
            limit=params.get("limit", 100)
        )
        return {
            "success": True,
            "decisions": [
                {
                    "id": d.id,
                    "model_name": d.model_name,
                    "symbol": d.symbol,
                    "action": d.action,
                    "quantity": d.quantity,
                    "price": d.price,
                    "confidence": d.confidence,
                    "reasoning": d.reasoning
                }
                for d in decisions
            ]
        }

    # =========================================================================
    # System Information
    # =========================================================================

    if action == "system_info":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys)
        return agent.get_system_info()

    if action == "list_tools":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys)
        tools = agent.list_available_tools()
        return {
            "success": True,
            "tools": tools,
            "categories": list(tools.keys()),
            "total": sum(len(v) for v in tools.values())
        }

    if action == "list_models":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys)
        providers = agent.list_available_models()
        return {"success": True, "providers": providers, "count": len(providers)}

    if action == "list_output_models":
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys)
        models = agent.list_output_models()
        return {"success": True, "models": models}

    # =========================================================================
    # Unknown Action
    # =========================================================================

    return {"success": False, "error": f"Unknown action: {action}"}


if __name__ == "__main__":
    result = main()
    print(result)
