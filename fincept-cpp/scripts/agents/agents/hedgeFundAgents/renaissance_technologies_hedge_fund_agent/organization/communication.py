"""
Inter-Agent Communication Protocols

Defines how agents communicate within Renaissance Technologies.
Implements structured message passing, escalation, and collaboration.
"""

from dataclasses import dataclass, field
from typing import List, Dict, Any, Optional, Callable
from enum import Enum
from datetime import datetime
import json


class MessageType(str, Enum):
    """Types of inter-agent messages"""
    REQUEST = "request"           # Asking for work/analysis
    RESPONSE = "response"         # Responding to request
    ESCALATION = "escalation"     # Escalating to superior
    APPROVAL = "approval"         # Requesting approval
    DECISION = "decision"         # Communicating decision
    ALERT = "alert"               # Urgent notification
    UPDATE = "update"             # Status update
    COLLABORATION = "collaboration"  # Peer-to-peer collaboration


class Priority(str, Enum):
    """Message priority levels"""
    CRITICAL = "critical"   # Immediate attention required
    HIGH = "high"           # Process soon
    NORMAL = "normal"       # Standard priority
    LOW = "low"             # When time permits


class MessageStatus(str, Enum):
    """Message processing status"""
    PENDING = "pending"
    IN_PROGRESS = "in_progress"
    COMPLETED = "completed"
    ESCALATED = "escalated"
    REJECTED = "rejected"


@dataclass
class AgentMessage:
    """
    Structured message between agents.
    All inter-agent communication uses this format.
    """
    # Required fields (no defaults) must come first
    id: str
    timestamp: str
    message_type: MessageType
    priority: Priority

    # Sender/Receiver
    from_agent: str
    to_agent: str

    # Content (required)
    subject: str
    body: str

    # Optional fields (with defaults) come after
    cc_agents: List[str] = field(default_factory=list)
    data: Dict[str, Any] = field(default_factory=dict)

    # Context
    thread_id: Optional[str] = None  # For conversation threading
    in_reply_to: Optional[str] = None
    references: List[str] = field(default_factory=list)

    # Status tracking
    status: MessageStatus = MessageStatus.PENDING
    requires_response: bool = False
    deadline: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary"""
        return {
            "id": self.id,
            "timestamp": self.timestamp,
            "message_type": self.message_type.value,
            "priority": self.priority.value,
            "from_agent": self.from_agent,
            "to_agent": self.to_agent,
            "cc_agents": self.cc_agents,
            "subject": self.subject,
            "body": self.body,
            "data": self.data,
            "thread_id": self.thread_id,
            "in_reply_to": self.in_reply_to,
            "references": self.references,
            "status": self.status.value,
            "requires_response": self.requires_response,
            "deadline": self.deadline,
        }

    def to_context_string(self) -> str:
        """Convert to string for LLM context"""
        return f"""
[MESSAGE: {self.message_type.value.upper()}]
From: {self.from_agent}
To: {self.to_agent}
Priority: {self.priority.value}
Subject: {self.subject}

{self.body}

{json.dumps(self.data, indent=2) if self.data else ""}
"""


@dataclass
class ConversationThread:
    """A thread of related messages (conversation)"""
    thread_id: str
    subject: str
    participants: List[str]
    messages: List[AgentMessage] = field(default_factory=list)
    status: str = "active"
    created_at: str = ""
    updated_at: str = ""

    def add_message(self, message: AgentMessage):
        """Add message to thread"""
        message.thread_id = self.thread_id
        self.messages.append(message)
        self.updated_at = datetime.utcnow().isoformat()
        if message.from_agent not in self.participants:
            self.participants.append(message.from_agent)

    def get_context(self, max_messages: int = 10) -> str:
        """Get thread context for LLM"""
        recent = self.messages[-max_messages:]
        return "\n---\n".join([m.to_context_string() for m in recent])


# =============================================================================
# MESSAGE TEMPLATES
# =============================================================================

def create_signal_request(
    from_agent: str,
    to_agent: str,
    ticker: str,
    analysis_type: str,
    context: Dict[str, Any]
) -> AgentMessage:
    """Create a signal analysis request"""
    return AgentMessage(
        id=f"msg_{datetime.utcnow().strftime('%Y%m%d%H%M%S')}_{from_agent}",
        timestamp=datetime.utcnow().isoformat(),
        message_type=MessageType.REQUEST,
        priority=Priority.NORMAL,
        from_agent=from_agent,
        to_agent=to_agent,
        subject=f"Signal Analysis Request: {ticker}",
        body=f"""Requesting {analysis_type} analysis for {ticker}.

Please analyze and provide your assessment following standard protocols.
Include statistical significance, confidence intervals, and any concerns.""",
        data={
            "ticker": ticker,
            "analysis_type": analysis_type,
            "context": context,
        },
        requires_response=True,
    )


def create_approval_request(
    from_agent: str,
    to_agent: str,
    subject: str,
    details: Dict[str, Any],
    deadline: Optional[str] = None
) -> AgentMessage:
    """Create an approval request"""
    return AgentMessage(
        id=f"msg_{datetime.utcnow().strftime('%Y%m%d%H%M%S')}_{from_agent}",
        timestamp=datetime.utcnow().isoformat(),
        message_type=MessageType.APPROVAL,
        priority=Priority.HIGH,
        from_agent=from_agent,
        to_agent=to_agent,
        subject=f"Approval Required: {subject}",
        body=f"""Requesting your approval for the following:

{json.dumps(details, indent=2)}

Please review and respond with APPROVED, REJECTED, or REQUEST_CHANGES.""",
        data=details,
        requires_response=True,
        deadline=deadline,
    )


def create_escalation(
    from_agent: str,
    to_agent: str,
    issue: str,
    context: Dict[str, Any],
    original_thread_id: Optional[str] = None
) -> AgentMessage:
    """Create an escalation message"""
    return AgentMessage(
        id=f"msg_{datetime.utcnow().strftime('%Y%m%d%H%M%S')}_{from_agent}",
        timestamp=datetime.utcnow().isoformat(),
        message_type=MessageType.ESCALATION,
        priority=Priority.HIGH,
        from_agent=from_agent,
        to_agent=to_agent,
        subject=f"ESCALATION: {issue}",
        body=f"""Escalating for your attention:

Issue: {issue}

This requires your decision as it exceeds my authority or involves
considerations outside my expertise.

Context:
{json.dumps(context, indent=2)}""",
        data=context,
        thread_id=original_thread_id,
        requires_response=True,
    )


def create_alert(
    from_agent: str,
    to_agents: List[str],
    alert_type: str,
    details: Dict[str, Any]
) -> AgentMessage:
    """Create an alert message"""
    return AgentMessage(
        id=f"msg_{datetime.utcnow().strftime('%Y%m%d%H%M%S')}_{from_agent}",
        timestamp=datetime.utcnow().isoformat(),
        message_type=MessageType.ALERT,
        priority=Priority.CRITICAL,
        from_agent=from_agent,
        to_agent=to_agents[0],
        cc_agents=to_agents[1:],
        subject=f"ALERT: {alert_type}",
        body=f"""IMMEDIATE ATTENTION REQUIRED

Alert Type: {alert_type}

{json.dumps(details, indent=2)}

Please acknowledge receipt and take appropriate action.""",
        data=details,
        requires_response=True,
    )


def create_decision(
    from_agent: str,
    to_agents: List[str],
    decision: str,
    rationale: str,
    details: Dict[str, Any],
    in_reply_to: Optional[str] = None
) -> AgentMessage:
    """Create a decision communication"""
    return AgentMessage(
        id=f"msg_{datetime.utcnow().strftime('%Y%m%d%H%M%S')}_{from_agent}",
        timestamp=datetime.utcnow().isoformat(),
        message_type=MessageType.DECISION,
        priority=Priority.HIGH,
        from_agent=from_agent,
        to_agent=to_agents[0],
        cc_agents=to_agents[1:],
        subject=f"Decision: {decision}",
        body=f"""Decision has been made:

{decision}

Rationale:
{rationale}

Details:
{json.dumps(details, indent=2)}

Please proceed accordingly.""",
        data=details,
        in_reply_to=in_reply_to,
    )


def create_collaboration_request(
    from_agent: str,
    to_agent: str,
    topic: str,
    question: str,
    context: Dict[str, Any]
) -> AgentMessage:
    """Create a peer collaboration request"""
    return AgentMessage(
        id=f"msg_{datetime.utcnow().strftime('%Y%m%d%H%M%S')}_{from_agent}",
        timestamp=datetime.utcnow().isoformat(),
        message_type=MessageType.COLLABORATION,
        priority=Priority.NORMAL,
        from_agent=from_agent,
        to_agent=to_agent,
        subject=f"Collaboration: {topic}",
        body=f"""Seeking your input on:

{question}

Context:
{json.dumps(context, indent=2)}

Your expertise would be valuable here. Happy to discuss further.""",
        data=context,
        requires_response=True,
    )


# =============================================================================
# COMMUNICATION MANAGER
# =============================================================================

class CommunicationManager:
    """
    Manages inter-agent communication.
    Handles message routing, threading, and history.
    """

    def __init__(self):
        self.threads: Dict[str, ConversationThread] = {}
        self.message_history: List[AgentMessage] = []
        self.pending_messages: Dict[str, List[AgentMessage]] = {}  # by agent

    def send_message(self, message: AgentMessage) -> str:
        """Send a message and return message ID"""
        # Add to history
        self.message_history.append(message)

        # Add to thread if exists
        if message.thread_id and message.thread_id in self.threads:
            self.threads[message.thread_id].add_message(message)

        # Add to pending for recipient
        if message.to_agent not in self.pending_messages:
            self.pending_messages[message.to_agent] = []
        self.pending_messages[message.to_agent].append(message)

        # Also add to CC recipients
        for cc in message.cc_agents:
            if cc not in self.pending_messages:
                self.pending_messages[cc] = []
            self.pending_messages[cc].append(message)

        return message.id

    def get_pending_messages(self, agent_id: str) -> List[AgentMessage]:
        """Get pending messages for an agent"""
        return self.pending_messages.get(agent_id, [])

    def mark_as_processed(self, message_id: str, status: MessageStatus):
        """Mark a message as processed"""
        for msg in self.message_history:
            if msg.id == message_id:
                msg.status = status
                break

    def create_thread(self, subject: str, participants: List[str]) -> str:
        """Create a new conversation thread"""
        thread_id = f"thread_{datetime.utcnow().strftime('%Y%m%d%H%M%S')}"
        self.threads[thread_id] = ConversationThread(
            thread_id=thread_id,
            subject=subject,
            participants=participants,
            created_at=datetime.utcnow().isoformat(),
            updated_at=datetime.utcnow().isoformat(),
        )
        return thread_id

    def get_thread_context(self, thread_id: str, max_messages: int = 10) -> str:
        """Get thread context for agent"""
        if thread_id not in self.threads:
            return ""
        return self.threads[thread_id].get_context(max_messages)

    def get_agent_conversation_history(
        self,
        agent_id: str,
        max_messages: int = 20
    ) -> List[AgentMessage]:
        """Get recent messages involving an agent"""
        relevant = [
            m for m in self.message_history
            if m.from_agent == agent_id or
               m.to_agent == agent_id or
               agent_id in m.cc_agents
        ]
        return relevant[-max_messages:]

    def format_for_context(
        self,
        messages: List[AgentMessage],
        include_data: bool = False
    ) -> str:
        """Format messages for LLM context injection"""
        parts = []
        for msg in messages:
            part = f"""
[{msg.timestamp}] {msg.message_type.value.upper()} from {msg.from_agent}
Subject: {msg.subject}
{msg.body}
"""
            if include_data and msg.data:
                part += f"\nData: {json.dumps(msg.data, indent=2)}"
            parts.append(part)

        return "\n---\n".join(parts)


# Global communication manager instance
_comm_manager: Optional[CommunicationManager] = None


def get_communication_manager() -> CommunicationManager:
    """Get the global communication manager"""
    global _comm_manager
    if _comm_manager is None:
        _comm_manager = CommunicationManager()
    return _comm_manager


def reset_communication_manager():
    """Reset the communication manager (for testing)"""
    global _comm_manager
    _comm_manager = None
