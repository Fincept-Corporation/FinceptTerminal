"""
UUID generation utilities for Alpha Arena.
"""

import time
import uuid
import random
import string


def generate_uuid(prefix: str = "") -> str:
    """Generate a unique identifier with optional prefix.

    Args:
        prefix: Optional prefix for the ID (e.g., "comp", "trade", "decision")

    Returns:
        Unique identifier string
    """
    base_id = str(uuid.uuid4())[:8]
    timestamp = int(time.time() * 1000)

    if prefix:
        return f"{prefix}_{timestamp}_{base_id}"
    return f"{timestamp}_{base_id}"


def generate_item_id() -> str:
    """Generate a unique item identifier for conversation items."""
    return generate_uuid("item")


def generate_competition_id() -> str:
    """Generate a unique competition identifier."""
    random_suffix = ''.join(random.choices(string.ascii_lowercase + string.digits, k=6))
    timestamp = int(time.time())
    return f"comp_{timestamp}_{random_suffix}"


def generate_cycle_id(competition_id: str, cycle_number: int) -> str:
    """Generate a unique cycle identifier."""
    timestamp = int(time.time() * 1000)
    return f"{competition_id}_cycle_{cycle_number}_{timestamp}"


def generate_decision_id(competition_id: str, model_name: str, cycle_number: int) -> str:
    """Generate a unique decision identifier."""
    timestamp = int(time.time() * 1000)
    return f"{competition_id}_{model_name}_{cycle_number}_{timestamp}"


def generate_snapshot_id(competition_id: str, model_name: str, cycle_number: int) -> str:
    """Generate a unique snapshot identifier."""
    timestamp = int(time.time() * 1000)
    return f"{competition_id}_{model_name}_snap_{cycle_number}_{timestamp}"
