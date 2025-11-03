
"""Machine Learning Kimik2 Module
======================================

Advanced machine learning trading models

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



import requests
import json
import os
import time
from typing import Optional, Dict, Any
from dataclasses import dataclass
from datetime import datetime

@dataclass
class OpenRouterConfig:
    """Configuration for OpenRouter API"""
    base_url: str = "https://openrouter.ai/api/v1"
    model: str = "moonshotai/kimi-k2:free"
    timeout: int = 30
    max_retries: int = 3
    retry_delay: float = 1.0

class OpenRouterClient:
    """Enhanced OpenRouter API client with error handling and retries"""
    
    def __init__(self, api_key: Optional[str] = None, config: Optional[OpenRouterConfig] = None):
        self.config = config or OpenRouterConfig()
        self.api_key = api_key or os.getenv("OPENROUTER_API_KEY")
        
        if not self.api_key:
            raise ValueError("API key required. Set OPENROUTER_API_KEY environment variable or pass api_key parameter.")
    
    def chat_completion(self, messages: list, **kwargs) -> Dict[str, Any]:
        """Send a chat completion request with retry logic"""
        
        url = f"{self.config.base_url}/chat/completions"
        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json",
        }
        
        payload = {
            "model": kwargs.get("model", self.config.model),
            "messages": messages,
        }
        
        # Add any additional parameters
        payload.update(kwargs)
        
        for attempt in range(self.config.max_retries):
            try:
                response = requests.post(
                    url=url,
                    headers=headers,
                    json=payload,
                    timeout=self.config.timeout
                )
                
                # Check for rate limiting
                if response.status_code == 429:
                    wait_time = int(response.headers.get("retry-after", attempt + 1))
                    print(f"Rate limited. Waiting {wait_time} seconds...")
                    time.sleep(wait_time)
                    continue
                
                # Raise an exception for bad status codes
                response.raise_for_status()
                
                result = response.json()
                
                # Validate response structure
                if "choices" not in result:
                    raise ValueError(f"Invalid response format: {result}")
                
                return result
                
            except requests.exceptions.RequestException as e:
                if attempt < self.config.max_retries - 1:
                    print(f"Request failed (attempt {attempt + 1}/{self.config.max_retries}): {e}")
                    time.sleep(self.config.retry_delay * (attempt + 1))
                else:
                    print(f"Request failed after {self.config.max_retries} attempts: {e}")
                    raise
            except json.JSONDecodeError as e:
                print(f"Failed to decode response: {e}")
                print(f"Raw response: {response.text}")
                raise
        
        raise RuntimeError("Maximum retries exceeded")
    
    def print_chat_response(self, response: Dict[str, Any]):
        """Pretty print the chat response"""
        
        print(f"Response received at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print("-" * 50)
        
        if "choices" in response and len(response["choices"]) > 0:
            choice = response["choices"][0]
            message = choice.get("message", {})
            
            print(f"Model: {response.get('model', 'Unknown')}")
            print(f"Role: {message.get('role', 'Unknown')}")
            
            content = message.get('content', 'No content returned')
            print(f"Content: {content}")
            
            # Show token usage if available
            if "usage" in response:
                usage = response["usage"]
                print(f"Token usage - Prompt: {usage.get('prompt_tokens', 0)}, "
                      f"Completion: {usage.get('completion_tokens', 0)}, "
                      f"Total: {usage.get('total_tokens', 0)}")
        else:
            print("No valid response received")
            print(f"Full response: {json.dumps(response, indent=2)}")
        
        print("-" * 50)

def main():
    """Main function demonstrating the improved client"""
    
    """
    Example usage demonstrating various API features
    """
    try:
        # Get API key from environment variable
        api_key = os.getenv("OPENROUTER_API_KEY")
        
        if not api_key:
            print("Error: OPENROUTER_API_KEY environment variable not set.")
            print("Please set your API key using: export OPENROUTER_API_KEY='your-key-here'")
            print("Or create a .env file with: OPENROUTER_API_KEY=your-key-here")
            print("\nFor testing, you can also initialize the client directly:")
            print("client = OpenRouterClient(api_key='your-key-here')")
            return
        
        # Initialize client with custom configuration
        config = OpenRouterConfig(
            timeout=60,  # Increase timeout for complex queries
            max_retries=5,  # More retries for reliability
        )
        
        client = OpenRouterClient(config=config)
        
        # Various example usages
        print("=== Example 1: Simple Question ===")
        messages1 = [
            {
                "role": "user",
                "content": "What is the meaning of life?"
            }
        ]
        
        response1 = client.chat_completion(messages1)
        client.print_chat_response(response1)
        
        print("\n=== Example 2: Multi-turn Conversation ===")
        messages2 = [
            {
                "role": "user",
                "content": "Explain quantum computing in simple terms"
            },
            {
                "role": "assistant", 
                "content": "Quantum computing is a type of computing that uses quantum physics principles to solve complex problems that classical computers struggle with. Would you like to know more about specific aspects?"
            },
            {
                "role": "user",
                "content": "Yes, tell me about quantum bits (qubits) and how they differ from regular bits."
            }
        ]
        
        response2 = client.chat_completion(messages2)
        client.print_chat_response(response2)
        
        print("\n=== Example 3: Custom Parameters ===")
        # Using different model and parameters
        messages3 = [
            {
                "role": "user",
                "content": "Generate a haiku about artificial intelligence"
            }
        ]
        
        response3 = client.chat_completion(messages3, 
                                         model="moonshotai/kimi-k2:free",
                                         max_tokens=50,
                                         temperature=0.8)
        client.print_chat_response(response3)
        
    except ValueError as e:
        print(f"Configuration error: {e}")
    except requests.exceptions.RequestException as e:
        print(f"Network error: {e}")
    except Exception as e:
        print(f"Unexpected error: {e}")
        traceback.print_exc()

if __name__ == "__main__":
    import traceback
    main()