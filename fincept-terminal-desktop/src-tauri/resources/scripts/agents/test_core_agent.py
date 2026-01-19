#!/usr/bin/env python3
"""
Test script for CoreAgent - verifies the single agent system works correctly
"""

import sys
import json
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent))

from finagent_core.core_agent import main


def test_basic_query():
    """Test basic query with minimal config"""
    print("=" * 60)
    print("TEST 1: Basic Query with OpenAI")
    print("=" * 60)

    query_json = json.dumps({
        "query": "What is 2+2? Answer in one sentence.",
        "session_id": "test-session-1"
    })

    config = json.dumps({
        "model": {
            "provider": "openai",
            "model_id": "gpt-3.5-turbo",
            "temperature": 0.7
        },
        "instructions": "You are a helpful math assistant.",
        "name": "Math Agent"
    })

    # Note: Use a real API key for actual testing
    api_keys = json.dumps({
        "OPENAI_API_KEY": "sk-test-key-here"
    })

    try:
        # New pattern: command + args
        result = main(["run", query_json, config, api_keys])
        result_data = json.loads(result)

        print(f"Success: {result_data.get('success')}")
        print(f"Response: {result_data.get('response', 'N/A')}")
        print(f"Session ID: {result_data.get('session_id')}")
        print()

    except Exception as e:
        print(f"Error: {str(e)}")
        print()


def test_with_tools():
    """Test agent with tools"""
    print("=" * 60)
    print("TEST 2: Agent with Calculator Tool")
    print("=" * 60)

    query_json = json.dumps({
        "query": "Calculate the square root of 144",
        "session_id": "test-session-2"
    })

    config = json.dumps({
        "model": {
            "provider": "openai",
            "model_id": "gpt-3.5-turbo"
        },
        "instructions": "You are a math assistant with calculator capabilities.",
        "tools": ["calculator"],
        "name": "Calculator Agent"
    })

    api_keys = json.dumps({
        "OPENAI_API_KEY": "sk-test-key-here"
    })

    try:
        # New pattern: command + args
        result = main(["run", query_json, config, api_keys])
        result_data = json.loads(result)

        print(f"Success: {result_data.get('success')}")
        print(f"Response: {result_data.get('response', 'N/A')}")
        print()

    except Exception as e:
        print(f"Error: {str(e)}")
        print()


def test_different_providers():
    """Test different LLM providers"""
    print("=" * 60)
    print("TEST 3: Testing Multiple Providers")
    print("=" * 60)

    providers = [
        {"provider": "openai", "model_id": "gpt-3.5-turbo", "key_name": "OPENAI_API_KEY"},
        {"provider": "anthropic", "model_id": "claude-3-haiku-20240307", "key_name": "ANTHROPIC_API_KEY"},
        {"provider": "ollama", "model_id": "llama3.3", "key_name": None},
    ]

    for i, provider_config in enumerate(providers, 1):
        print(f"\nProvider {i}: {provider_config['provider']}")
        print("-" * 40)

        query_json = json.dumps({
            "query": "Say hello in exactly 3 words.",
            "session_id": f"test-session-{i}"
        })

        config = json.dumps({
            "model": {
                "provider": provider_config["provider"],
                "model_id": provider_config["model_id"]
            },
            "instructions": "You are a concise assistant."
        })

        # Build API keys dict (only include if needed)
        api_keys_dict = {}
        if provider_config["key_name"]:
            api_keys_dict[provider_config["key_name"]] = "sk-test-key-here"
        api_keys = json.dumps(api_keys_dict)

        try:
            # New pattern: command + args
            result = main(["run", query_json, config, api_keys])
            result_data = json.loads(result)
            print(f"[OK] Success: {result_data.get('response', 'N/A')[:50]}...")
        except Exception as e:
            print(f"[ERROR] Error: {str(e)[:100]}...")


def test_config_changes():
    """Test that agent recreates itself when config changes"""
    print("=" * 60)
    print("TEST 4: Config Change Detection")
    print("=" * 60)

    # Same agent, different configs
    configs = [
        {
            "model": {"provider": "openai", "model_id": "gpt-3.5-turbo"},
            "instructions": "You are formal and professional.",
            "name": "Formal Agent"
        },
        {
            "model": {"provider": "openai", "model_id": "gpt-4"},
            "instructions": "You are casual and friendly.",
            "name": "Casual Agent"
        }
    ]

    for i, config in enumerate(configs, 1):
        print(f"\nConfig {i}: {config['name']}")
        print("-" * 40)

        query_json = json.dumps({
            "query": "Introduce yourself.",
            "session_id": f"test-session-config-{i}"
        })

        config_json = json.dumps(config)
        api_keys = json.dumps({"OPENAI_API_KEY": "sk-test-key-here"})

        try:
            # New pattern: command + args
            result = main(["run", query_json, config_json, api_keys])
            result_data = json.loads(result)
            print(f"Response: {result_data.get('response', 'N/A')[:100]}...")
        except Exception as e:
            print(f"Error: {str(e)[:100]}...")


def test_error_handling():
    """Test error handling"""
    print("=" * 60)
    print("TEST 5: Error Handling")
    print("=" * 60)

    # Test 1: Missing query
    print("\n1. Missing query field:")
    try:
        result = main([
            "run",
            json.dumps({"session_id": "test"}),
            json.dumps({"model": {"provider": "openai", "model_id": "gpt-3.5-turbo"}}),
            json.dumps({})
        ])
        result_data = json.loads(result)
        print(f"   Success: {result_data.get('success')}")
        print(f"   Error: {result_data.get('error')}")
    except Exception as e:
        print(f"   Exception: {str(e)[:100]}")

    # Test 2: Invalid JSON
    print("\n2. Invalid JSON:")
    try:
        result = main([
            "run",
            "not-valid-json",
            json.dumps({"model": {"provider": "openai", "model_id": "gpt-3.5-turbo"}}),
            json.dumps({})
        ])
        result_data = json.loads(result)
        print(f"   Success: {result_data.get('success')}")
        print(f"   Error: {result_data.get('error')[:80]}...")
    except Exception as e:
        print(f"   Exception: {str(e)[:100]}")

    # Test 3: Missing arguments
    print("\n3. Missing arguments:")
    try:
        result = main(["run", json.dumps({})])
        result_data = json.loads(result)
        print(f"   Success: {result_data.get('success')}")
        print(f"   Error: {result_data.get('error')}")
    except Exception as e:
        print(f"   Exception: {str(e)[:100]}")


if __name__ == "__main__":
    print("\n")
    print("=" * 60)
    print("CoreAgent Test Suite")
    print("=" * 60)
    print()

    print("NOTE: These tests require valid API keys to pass fully.")
    print("Replace 'sk-test-key-here' with real API keys for actual testing.")
    print()

    # Run all tests
    test_basic_query()
    test_with_tools()
    test_different_providers()
    test_config_changes()
    test_error_handling()

    print("\n")
    print("=" * 60)
    print("Test Suite Complete")
    print("=" * 60)
