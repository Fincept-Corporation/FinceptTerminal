# ===== HEDGE FUND AGENTS INTEGRATION GUIDE =====

"""
===== DATA SOURCES REQUIRED =====
# INPUT:
#   - ticker symbols (array)
#   - end_date (string)
#   - Mock financial data for testing
#   - API keys for different LLM providers
#   - Hedge fund agent selections and configurations
#
# OUTPUT:
#   - Consensus trading signals from multiple hedge funds
#   - Individual fund analysis results
#   - Web service endpoints with JSON responses
#   - Portfolio recommendations with confidence scores
#
# PARAMETERS:
#   - consensus_threshold: 60% agreement for bullish/bearish signals
#   - llm_providers: OpenRouter, Ollama, OpenAI configurations
#   - selected_funds: List of hedge funds to run analysis
#   - web_service_endpoints: REST API endpoints for analysis
"""

# 1. STANDALONE USAGE (Direct Import)
from bridgewater_hedge_fund import bridgewater_associates_agent
from renaissance_hedge_fund import renaissance_technologies_agent
from citadel_hedge_fund import citadel_hedge_fund_agent


# ... import all other agents

def analyze_stock_with_hedge_funds(ticker: str, financial_data: dict):
    """Use multiple hedge fund agents to analyze a stock"""

    # Create mock state (replace with your actual state structure)
    state = {
        "data": {
            "tickers": [ticker],
            "end_date": "2024-12-31",
            "analyst_signals": {}
        },
        "metadata": {"show_reasoning": True}
    }

    # Run multiple hedge fund analyses
    results = {}

    # Bridgewater Analysis
    bridgewater_result = bridgewater_associates_agent(state, "bridgewater")
    results["bridgewater"] = state["data"]["analyst_signals"]["bridgewater"]

    # Renaissance Analysis
    renaissance_result = renaissance_technologies_agent(state, "renaissance")
    results["renaissance"] = state["data"]["analyst_signals"]["renaissance"]

    # AQR Analysis
    aqr_result = aqr_capital_hedge_fund_agent(state, "aqr")
    results["aqr"] = state["data"]["analyst_signals"]["aqr"]

    return results


# 2. OPENROUTER INTEGRATION
import openai


def setup_openrouter_client():
    """Setup OpenRouter client"""
    return openai.OpenAI(
        base_url="https://openrouter.ai/api/v1",
        api_key="YOUR_OPENROUTER_API_KEY"
    )


def call_llm_openrouter(prompt, pydantic_model, agent_name, state, default_factory,
                        model="anthropic/claude-3.5-sonnet"):
    """Modified LLM call for OpenRouter"""
    client = setup_openrouter_client()

    try:
        response = client.chat.completions.create(
            model=model,
            messages=[
                {"role": "system", "content": prompt.messages[0].content},
                {"role": "user", "content": prompt.messages[1].content}
            ],
            temperature=0.1,
            max_tokens=2000
        )

        # Parse JSON response
        import json
        content = response.choices[0].message.content

        # Extract JSON from response
        start_idx = content.find('{')
        end_idx = content.rfind('}') + 1
        json_str = content[start_idx:end_idx]

        parsed_data = json.loads(json_str)
        return pydantic_model(**parsed_data)

    except Exception as e:
        print(f"OpenRouter API error: {e}")
        return default_factory()


# 3. OLLAMA LOCAL INTEGRATION
import requests
import json


def call_llm_ollama(prompt, pydantic_model, agent_name, state, default_factory, model="llama3.1:70b"):
    """Modified LLM call for Ollama local"""

    # Combine system and user messages
    full_prompt = f"System: {prompt.messages[0].content}\n\nHuman: {prompt.messages[1].content}\n\nAssistant:"

    try:
        response = requests.post(
            "http://localhost:11434/api/generate",
            json={
                "model": model,
                "prompt": full_prompt,
                "stream": False,
                "options": {
                    "temperature": 0.1,
                    "top_p": 0.9
                }
            },
            timeout=120
        )

        if response.status_code == 200:
            content = response.json()["response"]

            # Extract JSON from response
            start_idx = content.find('{')
            end_idx = content.rfind('}') + 1

            if start_idx >= 0 and end_idx > start_idx:
                json_str = content[start_idx:end_idx]
                parsed_data = json.loads(json_str)
                return pydantic_model(**parsed_data)
            else:
                raise ValueError("No valid JSON found in response")
        else:
            raise Exception(f"HTTP {response.status_code}: {response.text}")

    except Exception as e:
        print(f"Ollama API error: {e}")
        return default_factory()


# 4. CUSTOM LLM INTEGRATION WRAPPER
class HedgeFundLLMAdapter:
    """Adapter to use different LLM providers with hedge fund agents"""

    def __init__(self, provider="openai", **kwargs):
        self.provider = provider
        self.config = kwargs

    def call_llm(self, prompt, pydantic_model, agent_name, state, default_factory):
        """Route to appropriate LLM provider"""

        if self.provider == "openrouter":
            return call_llm_openrouter(
                prompt, pydantic_model, agent_name, state, default_factory,
                model=self.config.get("model", "anthropic/claude-3.5-sonnet")
            )
        elif self.provider == "ollama":
            return call_llm_ollama(
                prompt, pydantic_model, agent_name, state, default_factory,
                model=self.config.get("model", "llama3.1:70b")
            )
        elif self.provider == "openai":
            return self.call_openai_llm(prompt, pydantic_model, agent_name, state, default_factory)
        else:
            raise ValueError(f"Unsupported provider: {self.provider}")

    def call_openai_llm(self, prompt, pydantic_model, agent_name, state, default_factory):
        """OpenAI integration"""
        import openai

        client = openai.OpenAI(api_key=self.config.get("api_key"))

        try:
            response = client.chat.completions.create(
                model=self.config.get("model", "gpt-4"),
                messages=[
                    {"role": "system", "content": prompt.messages[0].content},
                    {"role": "user", "content": prompt.messages[1].content}
                ],
                temperature=0.1,
                max_tokens=2000
            )

            content = response.choices[0].message.content
            start_idx = content.find('{')
            end_idx = content.rfind('}') + 1
            json_str = content[start_idx:end_idx]

            parsed_data = json.loads(json_str)
            return pydantic_model(**parsed_data)

        except Exception as e:
            print(f"OpenAI API error: {e}")
            return default_factory()


# 5. USAGE EXAMPLES

def example_standalone_usage():
    """Example: Using hedge fund agents standalone"""

    # Mock financial data (replace with real data from your API)
    mock_state = {
        "data": {
            "tickers": ["AAPL"],
            "end_date": "2024-12-31",
            "analyst_signals": {}
        },
        "metadata": {"show_reasoning": True}
    }

    # Patch the call_llm function to use your preferred provider
    import bridgewater_hedge_fund
    llm_adapter = HedgeFundLLMAdapter(provider="openrouter", model="anthropic/claude-3.5-sonnet")
    bridgewater_hedge_fund.call_llm = llm_adapter.call_llm

    # Run analysis
    result = bridgewater_associates_agent(mock_state, "bridgewater_test")

    print("Bridgewater Analysis Results:")
    print(json.dumps(mock_state["data"]["analyst_signals"]["bridgewater_test"], indent=2))


def example_multi_hedge_fund_consensus():
    """Example: Get consensus from multiple hedge funds"""

    hedge_funds = [
        ("bridgewater", bridgewater_associates_agent),
        ("renaissance", renaissance_technologies_agent),
        ("aqr", aqr_capital_hedge_fund_agent),
        ("elliott", elliott_management_hedge_fund_agent)
    ]

    ticker = "MSFT"
    all_signals = {}

    # Setup LLM adapter
    llm_adapter = HedgeFundLLMAdapter(provider="ollama", model="llama3.1:70b")

    for fund_name, fund_agent in hedge_funds:
        # Patch LLM function for this fund
        fund_module = fund_agent.__module__
        import importlib
        module = importlib.import_module(fund_module)
        module.call_llm = llm_adapter.call_llm

        # Create state
        state = create_mock_state([ticker])

        # Run analysis
        try:
            fund_agent(state, f"{fund_name}_analysis")
            all_signals[fund_name] = state["data"]["analyst_signals"][f"{fund_name}_analysis"][ticker]
        except Exception as e:
            print(f"Error running {fund_name}: {e}")
            continue

    # Calculate consensus
    consensus = calculate_hedge_fund_consensus(all_signals)
    return consensus


def create_mock_state(tickers: list):
    """Create mock state for testing"""
    return {
        "data": {
            "tickers": tickers,
            "end_date": "2024-12-31",
            "analyst_signals": {}
        },
        "metadata": {"show_reasoning": True}
    }


def calculate_hedge_fund_consensus(signals: dict):
    """Calculate consensus from multiple hedge fund signals"""

    if not signals:
        return {"consensus": "neutral", "confidence": 0}

    # Count signals
    bullish_count = sum(1 for signal in signals.values() if signal.get("signal") == "bullish")
    bearish_count = sum(1 for signal in signals.values() if signal.get("signal") == "bearish")
    neutral_count = len(signals) - bullish_count - bearish_count

    total_funds = len(signals)

    # Determine consensus
    if bullish_count > total_funds * 0.6:
        consensus = "bullish"
        confidence = bullish_count / total_funds
    elif bearish_count > total_funds * 0.6:
        consensus = "bearish"
        confidence = bearish_count / total_funds
    else:
        consensus = "neutral"
        confidence = max(bullish_count, bearish_count, neutral_count) / total_funds

    # Calculate average confidence
    avg_confidence = sum(signal.get("confidence", 0) for signal in signals.values()) / total_funds

    return {
        "consensus": consensus,
        "consensus_strength": confidence,
        "average_confidence": avg_confidence,
        "fund_breakdown": {
            "bullish": bullish_count,
            "bearish": bearish_count,
            "neutral": neutral_count
        },
        "individual_signals": signals
    }


# 6. FLASK/FASTAPI WEB SERVICE EXAMPLE

from flask import Flask, request, jsonify

app = Flask(__name__)


@app.route('/analyze/<ticker>')
def analyze_ticker(ticker):
    """Web endpoint to analyze a ticker with all hedge funds"""

    try:
        # Setup LLM adapter based on query parameter
        provider = request.args.get('provider', 'openrouter')
        model = request.args.get('model', 'anthropic/claude-3.5-sonnet')

        llm_adapter = HedgeFundLLMAdapter(provider=provider, model=model)

        # Run analysis with selected hedge funds
        selected_funds = request.args.get('funds', 'bridgewater,aqr,renaissance').split(',')

        results = {}
        for fund_name in selected_funds:
            if fund_name in AVAILABLE_FUNDS:
                fund_agent = AVAILABLE_FUNDS[fund_name]

                # Patch LLM
                fund_module = importlib.import_module(fund_agent.__module__)
                fund_module.call_llm = llm_adapter.call_llm

                # Run analysis
                state = create_mock_state([ticker])
                fund_agent(state, f"{fund_name}_web")
                results[fund_name] = state["data"]["analyst_signals"][f"{fund_name}_web"][ticker]

        # Calculate consensus
        consensus = calculate_hedge_fund_consensus(results)

        return jsonify({
            "ticker": ticker,
            "consensus": consensus,
            "individual_analyses": results,
            "timestamp": "2024-12-31T00:00:00Z"
        })

    except Exception as e:
        return jsonify({"error": str(e)}), 500


# Available hedge funds mapping
AVAILABLE_FUNDS = {
    "bridgewater": bridgewater_associates_agent,
    "renaissance": renaissance_technologies_agent,
    "citadel": citadel_hedge_fund_agent,
    "two_sigma": two_sigma_hedge_fund_agent,
    "aqr": aqr_capital_hedge_fund_agent,
    "elliott": elliott_management_hedge_fund_agent,
    "millennium": millennium_management_hedge_fund_agent,
    "de_shaw": de_shaw_hedge_fund_agent,
    "pershing_square": pershing_square_hedge_fund_agent
}

if __name__ == "__main__":
    # Example usage
    print("=== Hedge Fund Agents Integration Examples ===")

    # Test standalone usage
    example_standalone_usage()

    # Test multi-fund consensus
    consensus = example_multi_hedge_fund_consensus()
    print("\nConsensus Results:")
    print(json.dumps(consensus, indent=2))

    # Start web service (uncomment to run)
    # app.run(debug=True, port=5000)

# 7. CONFIGURATION FILE EXAMPLE (config.yaml)
"""
# config.yaml
llm_providers:
  openrouter:
    api_key: "YOUR_OPENROUTER_KEY"
    base_url: "https://openrouter.ai/api/v1"
    default_model: "anthropic/claude-3.5-sonnet"

  ollama:
    base_url: "http://localhost:11434"
    default_model: "llama3.1:70b"

  openai:
    api_key: "YOUR_OPENAI_KEY" 
    default_model: "gpt-4"

financial_data:
  api_key: "YOUR_FINANCIAL_API_KEY"
  provider: "alpha_vantage"  # or "financial_datasets"

hedge_funds:
  enabled:
    - bridgewater
    - renaissance
    - aqr
    - elliott

  default_analysis_period: "annual"
  default_limit: 10
"""