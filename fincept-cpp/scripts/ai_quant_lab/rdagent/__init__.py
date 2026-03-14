"""
RD-Agent Extended Implementation
Modular implementation of Microsoft RD-Agent features
"""

from .core.hypothesis_gen import HypothesisGenerator
from .core.knowledge_base import KnowledgeBase
from .proposal.advanced_proposer import AdvancedProposer
from .proposal.checkpoint_selector import CheckpointSelector
from .proposal.sota_selector import SOTASelector
from .workflow.tracking import WorkflowTracker
from .workflow.loop_manager import LoopManager
from .experiments.selector import ExperimentSelector

__all__ = [
    'HypothesisGenerator',
    'KnowledgeBase',
    'AdvancedProposer',
    'CheckpointSelector',
    'SOTASelector',
    'WorkflowTracker',
    'LoopManager',
    'ExperimentSelector'
]
