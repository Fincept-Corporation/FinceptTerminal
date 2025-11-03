
"""Machine Learning Test Client Module
======================================

Testing client for ML trading strategies

===== DATA SOURCES REQUIRED =====
INPUT:
  - Historical market data and price time series
  - Technical indicators and market features
  - Alternative data sources (news, sentiment, etc.)
  - Market microstructure data and order flow
  - Economic indicators and macro data

OUTPUT:
  - Machine learning models for price prediction
  - Trading signals and strategy recommendations
  - Risk forecasts and position sizing suggestions
  - Performance metrics and backtest results
  - Feature importance and model interpretability

PARAMETERS:
  - model_type: Machine learning algorithm (default: 'random_forest')
  - training_window: Training data window size (default: 252 days)
  - prediction_horizon: Prediction horizon (default: 1 day)
  - feature_selection: Feature selection method (default: 'recursive')
  - validation_method: Model validation approach (default: 'time_series_split')
"""

#!/usr/bin/env python3
"""
Test script for the enhanced OpenRouter API client
Run this to verify your setup and test basic functionality
"""


import os
import sys
from dotenv import load_dotenv
from kimi2 import OpenRouterClient, OpenRouterConfig

def test_setup():
    """Test basic setup and configuration"""
    print("=== Testing OpenRouter Client Setup ===")
    
    # Load environment variables
    load_dotenv()
    
    # Check for API key
    api_key = os.getenv("OPENROUTER_API_KEY")
    if not api_key:
        print("❌ FAIL: OPENROUTER_API_KEY environment variable not found")
        print("Please create a .env file based on .env.example")
        return False
    else:
        print("✅ PASS: API key found in environment")
    
    # Test client initialization
    try:
        config = OpenRouterConfig(max_retries=1)  # Quick test with fewer retries
        client = OpenRouterClient(api_key=api_key, config=config)
        print("✅ PASS: Client initialization successful")
    except Exception as e:
        print(f"❌ FAIL: Client initialization failed: {e}")
        return False
    
    return True

def test_api_call():
    """Test actual API call with a simple request"""
    print("\n=== Testing API Call ===")
    
    try:
        # Load environment variables
        load_dotenv()
        api_key = os.getenv("OPENROUTER_API_KEY")
        
        if not api_key:
            print("❌ SKIPPED: API key not configured")
            return False
        
        # Initialize client with minimal config for testing
        config = OpenRouterConfig(timeout=30, max_retries=2)
        client = OpenRouterClient(api_key=api_key, config=config)
        
        # Make a simple test call
        messages = [{"role": "user", "content": "Say 'Hello from test client' and count to 3."}]
        
        print("Making test API call...")
        response = client.chat_completion(messages, max_tokens=50)
        
        # Validate response
        if response and "choices" in response:
            print("✅ PASS: API call successful")
            
            # Show response
            if len(response["choices"]) > 0:
                content = response["choices"][0].get("message", {}).get("content", "")
                print(f"Response: {content[:100]}...")  # First 100 chars
            return True
        else:
            print("❌ FAIL: Invalid response structure")
            return False
            
    except requests.exceptions.RequestException as e:
        print(f"❌ FAIL: Network error during API call: {e}")
        return False
    except Exception as e:
        print(f"❌ FAIL: Unexpected error during API call: {e}")
        return False

def test_error_handling():
    """Test error handling scenarios"""
    print("\n=== Testing Error Handling ===")
    
    try:
        # Test without API key
        try:
            client = OpenRouterClient()
            print("❌ FAIL: Should have raised ValueError without API key")
            return False
        except ValueError as e:
            print("✅ PASS: Correctly raises ValueError without API key")
        
        # Test with invalid API key
        try:
            config = OpenRouterConfig(timeout=5, max_retries=1)
            client = OpenRouterClient(api_key="invalid-key", config=config)
            messages = [{"role": "user", "content": "test"}]
            client.chat_completion(messages)
            print("❌ FAIL: Should have failed with invalid API key")
            return False
        except requests.exceptions.RequestException:
            print("✅ PASS: Correctly handles invalid API key")
        
        return True
        
    except Exception as e:
        print(f"❌ FAIL: Error handling test failed: {e}")
        return False

def main():
    """Run all tests"""
    print("OpenRouter Client Test Suite")
    print("=" * 50)
    
    tests = [
        ("Setup Test", test_setup),
        ("Error Handling Test", test_error_handling),
        ("API Call Test", test_api_call)
    ]
    
    results = []
    
    for test_name, test_func in tests:
        try:
            result = test_func()
            results.append((test_name, result))
        except Exception as e:
            print(f"❌ FAIL: {test_name} raised exception: {e}")
            results.append((test_name, False))
    
    # Summary
    print("\n" + "=" * 50)
    print("Test Results Summary:")
    print("=" * 50)
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for test_name, result in results:
        status = "✅ PASS" if result else "❌ FAIL"
        print(f"{status} {test_name}")
    
    print(f"\nResults: {passed}/{total} tests passed")
    
    if passed == total:
        print("🎉 All tests passed! Your setup is ready.")
        return 0
    else:
        print("⚠️  Some tests failed. Check the output above for details.")
        return 1

if __name__ == "__main__":
    sys.exit(main())