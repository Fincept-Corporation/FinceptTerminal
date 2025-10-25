

"""
===== DATA SOURCES REQUIRED =====
# INPUT:
#   - state dictionary with metadata
#   - api_key_name string identifier
#   - request object with api_keys attribute
#
# OUTPUT:
#   - API key string for authentication
#   - None if key not found
#
# PARAMETERS:
#   - state: Dictionary containing application state and request metadata
#   - api_key_name: String identifier for specific API key (e.g., "FINANCIAL_DATASETS_API_KEY")
"""


def get_api_key_from_state(state: dict, api_key_name: str) -> str:
    """Get an API key from the state object."""
    if state and state.get("metadata", {}).get("request"):
        request = state["metadata"]["request"]
        if hasattr(request, 'api_keys') and request.api_keys:
            return request.api_keys.get(api_key_name)
    return None