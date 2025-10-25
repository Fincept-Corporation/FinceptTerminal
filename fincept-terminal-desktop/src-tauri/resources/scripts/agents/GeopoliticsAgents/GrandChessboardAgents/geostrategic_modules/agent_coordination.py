"""
AGENT COORDINATION SYSTEM
Inter-Agent Communication and Data Sharing for Grand Chessboard Framework

This module implements a sophisticated coordination system that allows
Grand Chessboard agents to communicate, share data, and collaborate on
comprehensive strategic analysis through shared chessboard state management.
"""

import logging
from typing import Dict, List, Any, Optional, Tuple, Callable
from dataclasses import dataclass, field
from enum import Enum
from datetime import datetime
import asyncio
import threading
import queue
import json

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class MessageType(Enum):
    """Types of inter-agent messages"""
    DATA_SHARING = "data_sharing"           # Share geopolitical data
    INSIGHT_SHARING = "insight_sharing"     # Share strategic insights
    ANALYSIS_REQUEST = "analysis_request"     # Request specific analysis
    STATUS_UPDATE = "status_update"           # Update analysis status
    CONFLICT_ALERT = "conflict_alert"         # Alert about conflicting recommendations
    COORDINATION_REQUEST = "coordination_request"  # Request coordination on analysis
    RESPONSE = "response"                   # Response to previous message

class Priority(Enum):
    """Message priority levels"""
    CRITICAL = "critical"      # Urgent strategic intelligence
    HIGH = "high"             # Important coordination needs
    NORMAL = "normal"           # Regular information sharing
    LOW = "low"               # Background updates

@dataclass
class AgentMessage:
    """Inter-agent communication message"""
    id: str
    sender: str
    recipients: List[str]
    message_type: MessageType
    priority: Priority
    timestamp: datetime
    data: Dict[str, Any]
    metadata: Dict[str, Any] = field(default_factory=dict)
    requires_response: bool = False
    correlation_id: Optional[str] = None

@dataclass
class SharedChessboardState:
    """Shared state of Eurasian chessboard across all agents"""
    global_power_distribution: Dict[str, float] = field(default_factory=dict)
    regional_dynamics: Dict[str, Dict[str, Any]] = field(default_factory=dict)
    strategic_priorities: List[str] = field(default_factory=list)
    threat_assessments: Dict[str, str] = field(default_factory=dict)
    agent_insights: Dict[str, List[Dict[str, Any]]] = field(default_factory=dict)
    coordination_requests: List[Dict[str, Any]] = field(default_factory=list)
    last_updated: datetime = field(default_factory=datetime.now)

@dataclass
class CoordinationResult:
    """Result of coordinated analysis across multiple agents"""
    coordinated_by: str
    participating_agents: List[str]
    combined_insights: Dict[str, Any]
    consensus_recommendations: List[str]
    conflicting_recommendations: List[Dict[str, Any]]
    coordination_success: bool
    timestamp: datetime = field(default_factory=datetime.now)

class AgentCoordinationSystem:
    """
    Sophisticated agent coordination system enabling inter-agent communication
    and collaborative analysis through shared chessboard state management.

    Features:
    - Message passing between agents with priority queuing
    - Shared chessboard state with conflict resolution
    - Coordinated analysis requests and result aggregation
    - Conflict detection and resolution mechanisms
    - Agent status monitoring and health checks
    """

    def __init__(self):
        """Initialize the agent coordination system"""
        self.system_id = "agent_coordination_system"

        # Active agents registry
        self.registered_agents = {}

        # Message queues for each agent
        self.message_queues = {}

        # Shared chessboard state
        self.shared_state = SharedChessboardState()

        # Coordination history
        self.coordination_history = []

        # Message routing
        self.message_queue = queue.PriorityQueue()
        self.message_handlers = {}

        # Locking for thread safety
        self.state_lock = threading.Lock()
        self.coordination_lock = threading.Lock()

        # Configuration
        self.max_queue_size = 1000
        self.message_timeout = 300  # 5 minutes
        self.coordination_timeout = 600  # 10 minutes

        # Available Grand Chessboard agents
        self.available_agents = [
            "eurasian_chessboard_master",
            "democratic_bridgehead",
            "black_hole",
            "eurasian_balkans",
            "far_eastern_anchor",
            "geostrategic_players",
            "geopolitical_pivots",
            "american_hegemony"
        ]

    def register_agent(self, agent_id: str, message_handler: Callable[[AgentMessage], None]) -> bool:
        """
        Register an agent with the coordination system

        Args:
            agent_id: Unique identifier for the agent
            message_handler: Function to handle incoming messages

        Returns:
            True if registration successful, False otherwise
        """
        try:
            if agent_id in self.registered_agents:
                logger.warning(f"Agent {agent_id} already registered")
                return False

            if agent_id not in self.available_agents:
                logger.warning(f"Agent {agent_id} not recognized as Grand Chessboard agent")
                return False

            # Register the agent
            self.registered_agents[agent_id] = {
                "id": agent_id,
                "handler": message_handler,
                "status": "active",
                "last_seen": datetime.now(),
                "message_count": 0,
                "coordination_participations": 0
            }

            # Create message queue for the agent
            self.message_queues[agent_id] = queue.Queue()

            # Store message handler
            self.message_handlers[agent_id] = message_handler

            logger.info(f"Agent {agent_id} successfully registered with coordination system")
            return True

        except Exception as e:
            logger.error(f"Error registering agent {agent_id}: {str(e)}")
            return False

    def unregister_agent(self, agent_id: str) -> bool:
        """Unregister an agent from the coordination system"""
        try:
            if agent_id not in self.registered_agents:
                logger.warning(f"Agent {agent_id} not registered")
                return False

            # Remove agent registration
            del self.registered_agents[agent_id]
            del self.message_queues[agent_id]
            del self.message_handlers[agent_id]

            logger.info(f"Agent {agent_id} successfully unregistered")
            return True

        except Exception as e:
            logger.error(f"Error unregistering agent {agent_id}: {str(e)}")
            return False

    def send_message(
        self,
        sender: str,
        recipients: List[str],
        message_type: MessageType,
        priority: Priority,
        data: Dict[str, Any],
        metadata: Optional[Dict[str, Any]] = None,
        requires_response: bool = False,
        correlation_id: Optional[str] = None
    ) -> bool:
        """
        Send message from one agent to other agents

        Args:
            sender: ID of sending agent
            recipients: List of recipient agent IDs
            message_type: Type of message being sent
            priority: Priority level of message
            data: Message data payload
            metadata: Additional metadata
            requires_response: Whether response is required
            correlation_id: Correlation ID for request-response tracking

        Returns:
            True if message queued successfully, False otherwise
        """
        try:
            # Validate sender and recipients
            if sender not in self.registered_agents:
                logger.error(f"Sender {sender} not registered")
                return False

            for recipient in recipients:
                if recipient not in self.registered_agents:
                    logger.warning(f"Recipient {recipient} not registered")

            # Create message
            message = AgentMessage(
                id=self._generate_message_id(),
                sender=sender,
                recipients=recipients,
                message_type=message_type,
                priority=priority,
                timestamp=datetime.now(),
                data=data,
                metadata=metadata or {},
                requires_response=requires_response,
                correlation_id=correlation_id
            )

            # Calculate priority score for queue (lower = higher priority)
            priority_score = self._calculate_priority_score(priority)

            # Add to priority queue
            self.message_queue.put((priority_score, message))

            logger.info(f"Message from {sender} queued for {len(recipients)} recipients")
            return True

        except Exception as e:
            logger.error(f"Error sending message from {sender}: {str(e)}")
            return False

    def broadcast_insights(
        self,
        agent_id: str,
        insights: List[Dict[str, Any]],
        priority: Priority = Priority.NORMAL
    ) -> bool:
        """
        Broadcast strategic insights from an agent to all other agents

        Args:
            agent_id: ID of agent broadcasting insights
            insights: List of strategic insights to share
            priority: Priority level of broadcast

        Returns:
            True if broadcast successful, False otherwise
        """
        try:
            if agent_id not in self.registered_agents:
                logger.error(f"Agent {agent_id} not registered for insight broadcast")
                return False

            # Update shared state with new insights
            with self.state_lock:
                if agent_id not in self.shared_state.agent_insights:
                    self.shared_state.agent_insights[agent_id] = []

                self.shared_state.agent_insights[agent_id].extend(insights)
                self.shared_state.last_updated = datetime.now()

            # Broadcast to all other agents
            other_agents = [aid for aid in self.registered_agents.keys() if aid != agent_id]

            message_data = {
                "insights": insights,
                "agent_focus": self._get_agent_focus_area(agent_id),
                "strategic_implications": self._extract_strategic_implications(insights)
            }

            return self.send_message(
                sender=agent_id,
                recipients=other_agents,
                message_type=MessageType.INSIGHT_SHARING,
                priority=priority,
                data=message_data
            )

        except Exception as e:
            logger.error(f"Error broadcasting insights from {agent_id}: {str(e)}")
            return False

    def request_coordination(
        self,
        requesting_agent: str,
        target_agents: List[str],
        coordination_request: Dict[str, Any],
        priority: Priority = Priority.HIGH
    ) -> str:
        """
        Request coordination with specific agents for collaborative analysis

        Args:
            requesting_agent: ID of agent requesting coordination
            target_agents: List of agents to coordinate with
            coordination_request: Details of coordination request
            priority: Priority level of request

        Returns:
            Coordination ID for tracking the request
        """
        try:
            coordination_id = self._generate_coordination_id()

            # Add to coordination requests in shared state
            with self.coordination_lock:
                self.shared_state.coordination_requests.append({
                    "id": coordination_id,
                    "requesting_agent": requesting_agent,
                    "target_agents": target_agents,
                    "request_data": coordination_request,
                    "status": "pending",
                    "created_at": datetime.now(),
                    "priority": priority.value
                })

            # Send coordination request to target agents
            message_data = {
                "coordination_id": coordination_id,
                "coordination_request": coordination_request,
                "requested_by": requesting_agent,
                "coordination_type": coordination_request.get("type", "collaborative_analysis"),
                "analysis_focus": coordination_request.get("focus", "regional_balance"),
                "timeline": coordination_request.get("timeline", "immediate")
            }

            success = self.send_message(
                sender=requesting_agent,
                recipients=target_agents,
                message_type=MessageType.COORDINATION_REQUEST,
                priority=priority,
                data=message_data,
                requires_response=True
            )

            if success:
                logger.info(f"Coordination request {coordination_id} sent to {len(target_agents)} agents")
                return coordination_id
            else:
                return None

        except Exception as e:
            logger.error(f"Error requesting coordination from {requesting_agent}: {str(e)}")
            return None

    def detect_conflicts(self) -> List[Dict[str, Any]]:
        """
        Detect conflicting recommendations or analyses across agents

        Returns:
            List of detected conflicts with details
        """
        conflicts = []

        try:
            # Get latest insights from all agents
            all_insights = {}
            with self.state_lock:
                for agent_id, insights in self.shared_state.agent_insights.items():
                    if insights:
                        # Get most recent insights
                        recent_insights = sorted(insights, key=lambda x: x.get("timestamp", datetime.min), reverse=True)
                        if recent_insights:
                            all_insights[agent_id] = recent_insights[0]

            # Check for conflicts in recommendations
            recommendations = {}
            for agent_id, insight in all_insights.items():
                if "recommendations" in insight.get("data", {}):
                    recommendations[agent_id] = insight["data"]["recommendations"]

            # Find conflicting recommendations
            conflicts = self._identify_recommendation_conflicts(recommendations)

            # Check for conflicting strategic assessments
            assessments = {}
            for agent_id, insight in all_insights.items():
                if "strategic_assessment" in insight.get("data", {}):
                    assessments[agent_id] = insight["data"]["strategic_assessment"]

            strategic_conflicts = self._identify_strategic_conflicts(assessments)

            # Combine all conflicts
            all_conflicts = conflicts + strategic_conflicts

            # Alert relevant agents about conflicts
            if all_conflicts:
                self._broadcast_conflict_alerts(all_conflicts)

            return all_conflicts

        except Exception as e:
            logger.error(f"Error detecting conflicts: {str(e)}")
            return []

    def process_coordination_results(
        self,
        coordination_id: str,
        participating_agents: List[str],
        individual_results: Dict[str, Dict[str, Any]]
    ) -> CoordinationResult:
        """
        Process and synthesize results from coordinated analysis

        Args:
            coordination_id: ID of coordination request
            participating_agents: List of agents that participated
            individual_results: Individual analysis results from each agent

        Returns:
            Coordinated analysis result with synthesis
        """
        try:
            # Update coordination request status
            with self.coordination_lock:
                for req in self.shared_state.coordination_requests:
                    if req["id"] == coordination_id:
                        req["status"] = "completed"
                        req["completed_at"] = datetime.now()
                        break

            # Synthesize individual results
            combined_insights = self._synthesize_agent_results(individual_results)

            # Generate consensus recommendations
            consensus_recommendations = self._generate_consensus_recommendations(individual_results)

            # Identify conflicting recommendations
            conflicting_recommendations = self._identify_conflicting_recommendations(individual_results)

            # Create coordination result
            result = CoordinationResult(
                coordinated_by=participing_agents[0] if participating_agents else "unknown",
                participating_agents=participating_agents,
                combined_insights=combined_insights,
                consensus_recommendations=consensus_recommendations,
                conflicting_recommendations=conflicting_recommendations,
                coordination_success=len(participating_agents) > 1
            )

            # Store in coordination history
            self.coordination_history.append(result)

            # Broadcast result to all participating agents
            self._broadcast_coordination_result(coordination_id, result)

            logger.info(f"Coordination {coordination_id} completed with {len(participating_agents)} agents")
            return result

        except Exception as e:
            logger.error(f"Error processing coordination results for {coordination_id}: {str(e)}")
            return CoordinationResult(
                coordinated_by="error",
                participating_agents=[],
                combined_insights={},
                consensus_recommendations=["Coordination processing failed"],
                conflicting_recommendations=[],
                coordination_success=False
            )

    def get_shared_chessboard_state(self) -> SharedChessboardState:
        """Get current shared chessboard state"""
        with self.state_lock:
            return self.shared_state

    def get_agent_status(self, agent_id: Optional[str] = None) -> Dict[str, Any]:
        """Get status of specific agent or all agents"""
        if agent_id:
            if agent_id in self.registered_agents:
                return self.registered_agents[agent_id]
            else:
                return {"error": f"Agent {agent_id} not registered"}
        else:
            return self.registered_agents.copy()

    def get_coordination_history(self, limit: Optional[int] = None) -> List[CoordinationResult]:
        """Get coordination history"""
        if limit:
            return self.coordination_history[-limit:]
        return self.coordination_history.copy()

    # Private helper methods
    def _generate_message_id(self) -> str:
        """Generate unique message ID"""
        import uuid
        return str(uuid.uuid4())

    def _generate_coordination_id(self) -> str:
        """Generate unique coordination ID"""
        import uuid
        return f"coord_{str(uuid.uuid4())[:8]}"

    def _calculate_priority_score(self, priority: Priority) -> int:
        """Calculate priority score for queue (lower = higher priority)"""
        priority_scores = {
            Priority.CRITICAL: 1,
            Priority.HIGH: 2,
            Priority.NORMAL: 3,
            Priority.LOW: 4
        }
        return priority_scores.get(priority, 3)

    def _get_agent_focus_area(self, agent_id: str) -> str:
        """Get primary focus area for an agent"""
        agent_focuses = {
            "eurasian_chessboard_master": "global_balance",
            "democratic_bridgehead": "european_integration",
            "black_hole": "post_soviet_russia",
            "eurasian_balkans": "central_asia_caucasus",
            "far_eastern_anchor": "east_asian_balance",
            "geostrategic_players": "great_power_dynamics",
            "geopolitical_pivots": "pivot_state_leverage",
            "american_hegemony": "us_primacy_maintenance"
        }
        return agent_focuses.get(agent_id, "unknown")

    def _extract_strategic_implications(self, insights: List[Dict[str, Any]]) -> List[str]:
        """Extract strategic implications from insights"""
        implications = []
        for insight in insights:
            if "implications" in insight:
                if isinstance(insight["implications"], list):
                    implications.extend(insight["implications"])
                else:
                    implications.append(insight["implications"])
        return implications

    def _identify_recommendation_conflicts(self, recommendations: Dict[str, List[str]]) -> List[Dict[str, Any]]:
        """Identify conflicting recommendations across agents"""
        conflicts = []

        # Convert recommendations to sets for comparison
        rec_sets = {agent: set(recs) for agent, recs in recommendations.items()}

        # Check for opposing recommendations
        opposing_pairs = [
            ("escalate", "deescalate"),
            ("contain", "engage"),
            ("confront", "cooperate"),
            ("isolate", "integrate"),
            ("military_solution", "diplomatic_solution")
        ]

        for agent1, recs1 in recommendations.items():
            for agent2, recs2 in recommendations.items():
                if agent1 >= agent2:  # Avoid duplicate comparisons
                    continue

                for opposing_pair in opposing_pairs:
                    if any(opposing_pair[0] in rec for rec in [recs1, recs2]) and \
                       any(opposing_pair[1] in rec for rec in [recs1, recs2]):
                        conflicts.append({
                            "type": "opposing_recommendations",
                            "agents": [agent1, agent2],
                            "conflicting_recommendations": [
                                {"agent": agent1, "recs": [r for r in recs1 if opposing_pair[0] in r]},
                                {"agent": agent2, "recs": [r for r in recs2 if opposing_pair[1] in r]}
                            ],
                            "severity": "high"
                        })

        return conflicts

    def _identify_strategic_conflicts(self, assessments: Dict[str, Dict[str, Any]]) -> List[Dict[str, Any]]:
        """Identify conflicting strategic assessments"""
        conflicts = []

        # Check for conflicting stability assessments
        stability_assessments = {}
        for agent_id, assessment in assessments.items():
            if "stability_level" in assessment:
                stability_assessments[agent_id] = assessment["stability_level"]

        # Check for opposing stability views
        if "stable" in stability_assessments.values() and "unstable" in stability_assessments.values():
            stable_agents = [a for a, s in stability_assessments.items() if s == "stable"]
            unstable_agents = [a for a, s in stability_assessments.items() if s == "unstable"]

            conflicts.append({
                "type": "stability_assessment_conflict",
                "agents": stable_agents + unstable_agents,
                "conflict": "Different views on regional stability",
                "severity": "medium"
            })

        return conflicts

    def _broadcast_conflict_alerts(self, conflicts: List[Dict[str, Any]]) -> None:
        """Broadcast conflict alerts to relevant agents"""
        try:
            # Identify all agents involved in conflicts
            involved_agents = set()
            for conflict in conflicts:
                involved_agents.update(conflict.get("agents", []))

            if involved_agents:
                message_data = {
                    "conflicts": conflicts,
                    "alert_level": "high" if any(c.get("severity") == "high" for c in conflicts) else "medium",
                    "resolution_suggestion": "Coordinate to resolve conflicting assessments",
                    "requires_coordination": True
                }

                self.send_message(
                    sender="coordination_system",
                    recipients=list(involved_agents),
                    message_type=MessageType.CONFLICT_ALERT,
                    priority=Priority.HIGH,
                    data=message_data
                )

        except Exception as e:
            logger.error(f"Error broadcasting conflict alerts: {str(e)}")

    def _synthesize_agent_results(self, individual_results: Dict[str, Dict[str, Any]]) -> Dict[str, Any]:
        """Synthesize individual agent results into combined insights"""
        combined_insights = {
            "participating_agents": list(individual_results.keys()),
            "analysis_domains": {},
            "key_findings": [],
            "common_themes": [],
            "data_sources": []
        }

        # Extract analysis domains and key findings
        for agent_id, result in individual_results.items():
            if "assessment_type" in result:
                combined_insights["analysis_domains"][agent_id] = result["assessment_type"]

            if "key_findings" in result:
                combined_insights["key_findings"].extend(result["key_findings"])

            if "data_sources" in result:
                combined_insights["data_sources"].extend(result["data_sources"])

        # Identify common themes across agents
        all_recommendations = []
        for result in individual_results.values():
            if "strategic_recommendations" in result:
                all_recommendations.extend(result["strategic_recommendations"])

        # Find common themes using keyword analysis
        common_themes = self._extract_common_themes(all_recommendations)
        combined_insights["common_themes"] = common_themes

        return combined_insights

    def _generate_consensus_recommendations(self, individual_results: Dict[str, Dict[str, Any]]) -> List[str]:
        """Generate consensus recommendations from individual agent results"""
        all_recommendations = []

        # Collect all recommendations
        for result in individual_results.values():
            if "strategic_recommendations" in result:
                all_recommendations.extend(result["strategic_recommendations"])

        # Count frequency of similar recommendations
        recommendation_counts = {}
        for rec in all_recommendations:
            # Simple similarity check - could be enhanced with NLP
            similar_recs = [r for r in recommendation_counts.keys()
                            if self._are_recommendations_similar(r, rec)]

            if similar_recs:
                # Use existing similar recommendation key
                existing_key = similar_recs[0]
                recommendation_counts[existing_key] += 1
            else:
                recommendation_counts[rec] = 1

        # Sort by frequency and return top recommendations
        sorted_recommendations = sorted(recommendation_counts.items(),
                                   key=lambda x: x[1], reverse=True)

        consensus_recommendations = [rec for rec, count in sorted_recommendations[:5]]

        return consensus_recommendations

    def _identify_conflicting_recommendations(self, individual_results: Dict[str, Dict[str, Any]]) -> List[Dict[str, Any]]:
        """Identify recommendations that conflict across agents"""
        conflicts = []
        all_recommendations = {}

        # Collect all recommendations by agent
        for agent_id, result in individual_results.items():
            if "strategic_recommendations" in result:
                all_recommendations[agent_id] = result["strategic_recommendations"]

        # Use the conflict detection logic from earlier
        return self._identify_recommendation_conflicts(all_recommendations)

    def _extract_common_themes(self, recommendations: List[str]) -> List[str]:
        """Extract common themes from list of recommendations"""
        # Simple keyword-based theme extraction
        theme_keywords = {
            "balance_of_power": ["balance", "equilibrium", "power", "hegemony", "containment"],
            "alliance_cooperation": ["alliance", "cooperation", "partnership", "coordination"],
            "economic_strategy": ["economic", "trade", "investment", "development"],
            "military_deterrence": ["military", "deterrence", "defense", "security"],
            "diplomatic_engagement": ["diplomatic", "engagement", "dialogue", "negotiation"]
        }

        theme_counts = {theme: 0 for theme in theme_keywords}

        for rec in recommendations:
            rec_lower = rec.lower()
            for theme, keywords in theme_keywords.items():
                if any(keyword in rec_lower for keyword in keywords):
                    theme_counts[theme] += 1

        # Return themes with most mentions
        sorted_themes = sorted(theme_counts.items(), key=lambda x: x[1], reverse=True)
        return [theme for theme, count in sorted_themes if count > 0]

    def _are_recommendations_similar(self, rec1: str, rec2: str) -> bool:
        """Check if two recommendations are similar"""
        # Simple similarity based on shared keywords
        rec1_words = set(rec1.lower().split())
        rec2_words = set(rec2.lower().split())

        # Check if they share significant keywords
        common_words = rec1_words & rec2_words
        total_words = rec1_words | rec2_words

        # Consider similar if they share >30% of words
        similarity_ratio = len(common_words) / len(total_words) if total_words else 0
        return similarity_ratio > 0.3

    def _broadcast_coordination_result(self, coordination_id: str, result: CoordinationResult) -> None:
        """Broadcast coordination result to participating agents"""
        try:
            message_data = {
                "coordination_id": coordination_id,
                "result": result,
                "success": result.coordination_success,
                "consensus_count": len(result.consensus_recommendations),
                "conflict_count": len(result.conflicting_recommendations)
            }

            self.send_message(
                sender="coordination_system",
                recipients=result.participating_agents,
                message_type=MessageType.RESPONSE,
                priority=Priority.NORMAL,
                data=message_data
            )

        except Exception as e:
            logger.error(f"Error broadcasting coordination result: {str(e)}")

    def start_message_processing(self) -> None:
        """Start background thread for processing messages"""
        def process_messages():
            while True:
                try:
                    if not self.message_queue.empty():
                        _, message = self.message_queue.get()
                        self._route_message(message)
                    else:
                        import time
                        time.sleep(0.1)  # Small delay to prevent busy waiting
                except Exception as e:
                    logger.error(f"Error in message processing: {str(e)}")
                    import time
                    time.sleep(1)

        # Start background thread
        processing_thread = threading.Thread(target=process_messages, daemon=True)
        processing_thread.start()
        logger.info("Message processing thread started")

    def _route_message(self, message: AgentMessage) -> None:
        """Route message to appropriate recipients"""
        try:
            for recipient in message.recipients:
                if recipient in self.message_queues:
                    self.message_queues[recipient].put(message)

                    # Update agent status
                    if recipient in self.registered_agents:
                        self.registered_agents[recipient]["message_count"] += 1
                        self.registered_agents[recipient]["last_seen"] = datetime.now()

                        # Call message handler if exists
                        if recipient in self.message_handlers:
                            try:
                                self.message_handlers[recipient](message)
                            except Exception as e:
                                logger.error(f"Error in message handler for {recipient}: {str(e)}")
                else:
                    logger.warning(f"Recipient {recipient} not found in message queues")

        except Exception as e:
            logger.error(f"Error routing message: {str(e)}")

# Global coordination system instance
_coordination_system = None

def get_coordination_system() -> AgentCoordinationSystem:
    """Get or create the global coordination system instance"""
    global _coordination_system
    if _coordination_system is None:
        _coordination_system = AgentCoordinationSystem()
        _coordination_system.start_message_processing()
    return _coordination_system

# Example usage and integration functions
def register_chessboard_agent(agent_id: str, message_handler: Callable[[AgentMessage], None]) -> bool:
    """Convenience function to register a Grand Chessboard agent"""
    coordination_system = get_coordination_system()
    return coordination_system.register_agent(agent_id, message_handler)

def share_agent_insights(agent_id: str, insights: List[Dict[str, Any]], priority: Priority = Priority.NORMAL) -> bool:
    """Convenience function for agents to share insights"""
    coordination_system = get_coordination_system()
    return coordination_system.broadcast_insights(agent_id, insights, priority)

def request_agent_coordination(
    requesting_agent: str,
    target_agents: List[str],
    coordination_request: Dict[str, Any],
    priority: Priority = Priority.HIGH
) -> Optional[str]:
    """Convenience function to request coordination with other agents"""
    coordination_system = get_coordination_system()
    return coordination_system.request_coordination(
        requesting_agent, target_agents, coordination_request, priority
    )

def get_shared_chessboard_state() -> SharedChessboardState:
    """Convenience function to get current shared chessboard state"""
    coordination_system = get_coordination_system()
    return coordination_system.get_shared_chessboard_state()

# Example usage for testing
if __name__ == "__main__":
    # Example coordination system usage
    def sample_message_handler(message: AgentMessage) -> None:
        print(f"Agent {message.sender} sent message type {message.message_type}")
        print(f"Message data: {message.data}")

    # Register agents
    coordination_system = get_coordination_system()
    coordination_system.register_agent("democratic_bridgehead", sample_message_handler)
    coordination_system.register_agent("black_hole", sample_message_handler)
    coordination_system.register_agent("eurasian_balkans", sample_message_handler)

    # Example insight sharing
    insights = [
        {"finding": "NATO expansion enhances European stability", "impact": "high"},
        {"finding": "Chinese influence increasing in Central Asia", "impact": "medium"}
    ]

    share_agent_insights("democratic_bridgehead", insights)

    # Example coordination request
    coordination_request = {
        "type": "collaborative_analysis",
        "focus": "eurasian_balance_of_power",
        "timeline": "2_days",
        "specific_question": "How will NATO expansion affect Russian strategic calculations?"
    }

    coordination_id = request_agent_coordination(
        requesting_agent="eurasian_chessboard_master",
        target_agents=["democratic_bridgehead", "black_hole"],
        coordination_request=coordination_request
    )

    print(f"Coordination requested with ID: {coordination_id}")
    print("Coordination system active and processing messages")