#!/usr/bin/env python3
"""
LLM Models Service
Provides LLM model and provider information from embedded JSON data.
No external dependencies required - uses pre-extracted data from LiteLLM.
Supports 52+ providers and 1700+ models.
"""

import json
import sys
from pathlib import Path
from typing import Any


# Load embedded model data
def _load_model_data() -> dict:
    """Load the embedded model data from JSON file."""
    script_dir = Path(__file__).parent
    data_file = script_dir / "llm_models_data.json"

    if data_file.exists():
        with open(data_file, 'r', encoding='utf-8') as f:
            return json.load(f)

    # Fallback: try to load from litellm if JSON doesn't exist
    try:
        from litellm import models_by_provider, model_cost
        return {
            'providers': dict(models_by_provider),
            'model_info': {
                model_id: {
                    'max_tokens': cost.get('max_tokens') or cost.get('max_input_tokens', 0),
                    'input_cost': cost.get('input_cost_per_token', 0),
                    'output_cost': cost.get('output_cost_per_token', 0)
                }
                for model_id, cost in model_cost.items()
            }
        }
    except ImportError:
        return {'providers': {}, 'model_info': {}}


# Cache the loaded data
_MODEL_DATA = None

def get_model_data() -> dict:
    """Get cached model data."""
    global _MODEL_DATA
    if _MODEL_DATA is None:
        _MODEL_DATA = _load_model_data()
    return _MODEL_DATA


def get_all_providers() -> list[dict[str, Any]]:
    """Get all available providers with their model counts."""
    try:
        data = get_model_data()
        providers = []

        for provider, models in sorted(data['providers'].items()):
            providers.append({
                "id": provider,
                "name": provider.replace("_", " ").title(),
                "model_count": len(models)
            })

        return providers
    except Exception as e:
        return [{"error": str(e)}]


def get_models_by_provider(provider: str) -> list[dict[str, Any]]:
    """Get all models for a specific provider."""
    try:
        data = get_model_data()
        models = data['providers'].get(provider, [])
        model_info = data['model_info']

        result = []
        for model_id in models:
            info = model_info.get(model_id, {})

            # Clean up model name for display
            display_name = model_id
            if "/" in model_id:
                display_name = model_id.split("/")[-1]

            model_data = {
                "id": model_id,
                "name": display_name,
                "provider": provider,
                "context_window": info.get('max_tokens', 0),
                "input_cost_per_token": info.get('input_cost', 0),
                "output_cost_per_token": info.get('output_cost', 0),
            }

            # Add description based on model name patterns
            if "embedding" in model_id.lower():
                model_data["description"] = "Embedding model"
            elif "vision" in model_id.lower() or "image" in model_id.lower():
                model_data["description"] = "Vision/Image model"
            elif "code" in model_id.lower() or "coder" in model_id.lower():
                model_data["description"] = "Code generation model"
            elif "instruct" in model_id.lower():
                model_data["description"] = "Instruction-tuned model"
            else:
                model_data["description"] = f"{provider.replace('_', ' ').title()} model"

            result.append(model_data)

        # Sort by model name
        result.sort(key=lambda x: x["name"])

        return result
    except Exception as e:
        return [{"error": str(e)}]


def get_all_models() -> list[dict[str, Any]]:
    """Get all models from all providers."""
    try:
        data = get_model_data()
        model_info = data['model_info']

        result = []
        for provider, models in data['providers'].items():
            for model_id in models:
                info = model_info.get(model_id, {})

                display_name = model_id
                if "/" in model_id:
                    display_name = model_id.split("/")[-1]

                result.append({
                    "id": model_id,
                    "name": display_name,
                    "provider": provider,
                    "context_window": info.get('max_tokens', 0),
                })

        # Sort by provider then model name
        result.sort(key=lambda x: (x["provider"], x["name"]))

        return result
    except Exception as e:
        return [{"error": str(e)}]


def get_provider_stats() -> dict[str, Any]:
    """Get statistics about all providers."""
    try:
        data = get_model_data()
        providers = data['providers']

        total_models = sum(len(v) for v in providers.values())
        total_providers = len(providers)

        # Top providers by model count
        sorted_providers = sorted(
            providers.items(),
            key=lambda x: len(x[1]),
            reverse=True
        )[:15]

        return {
            "total_providers": total_providers,
            "total_models": total_models,
            "top_providers": [
                {"provider": p, "model_count": len(m)}
                for p, m in sorted_providers
            ]
        }
    except Exception as e:
        return {"error": str(e)}


def search_models(query: str) -> list[dict[str, Any]]:
    """Search models by name or provider."""
    try:
        data = get_model_data()
        model_info = data['model_info']

        query_lower = query.lower()
        result = []

        for provider, models in data['providers'].items():
            for model_id in models:
                if query_lower in model_id.lower() or query_lower in provider.lower():
                    info = model_info.get(model_id, {})

                    display_name = model_id
                    if "/" in model_id:
                        display_name = model_id.split("/")[-1]

                    result.append({
                        "id": model_id,
                        "name": display_name,
                        "provider": provider,
                        "context_window": info.get('max_tokens', 0),
                    })

        result.sort(key=lambda x: (x["provider"], x["name"]))
        return result[:100]  # Limit to 100 results
    except Exception as e:
        return [{"error": str(e)}]


def main():
    if len(sys.argv) < 2:
        print(json.dumps({"error": "No command specified"}))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "providers":
            result = get_all_providers()
        elif command == "models":
            if len(sys.argv) < 3:
                result = get_all_models()
            else:
                provider = sys.argv[2]
                result = get_models_by_provider(provider)
        elif command == "stats":
            result = get_provider_stats()
        elif command == "search":
            if len(sys.argv) < 3:
                result = {"error": "No search query specified"}
            else:
                query = sys.argv[2]
                result = search_models(query)
        else:
            result = {"error": f"Unknown command: {command}"}

        print(json.dumps(result))
    except Exception as e:
        print(json.dumps({"error": str(e)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
