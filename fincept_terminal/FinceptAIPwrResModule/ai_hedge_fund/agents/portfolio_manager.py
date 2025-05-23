# filename: portfolio_management_agent.py

import json
import logging
import time
import os

# Conditional imports based on selected model provider
try:
    import google.generativeai as genai  # Google Gemini
except ImportError:
    genai = None

try:
    import ollama  # Ollama LLMs (DeepSeek R1, Llama, etc.)
except ImportError:
    ollama = None

from pydantic import BaseModel, Field
from typing_extensions import Literal
from langchain_core.messages import HumanMessage
from langchain_core.prompts import ChatPromptTemplate

from fincept_terminal.FinceptAIPwrResModule.ai_hedge_fund.data.state import AgentState, show_agent_reasoning
from fincept_terminal.FinceptAIPwrResModule.ai_hedge_fund.utils.progress import progress

# ✅ Logging configuration
logger = logging.getLogger(__name__)

# ✅ Model Selection Constants
SUPPORTED_MODELS = {
    "gemini": "models/gemini-1.5-flash",
    "openai": "gpt-4-turbo",
    "ollama": "deepseek"
}


class PortfolioDecision(BaseModel):
    action: Literal["buy", "sell", "hold"]
    quantity: int = Field(description="Number of shares to trade")
    confidence: float = Field(description="Confidence, 0.0 to 100.0")
    reasoning: str = Field(description="Decision rationale")


class PortfolioManagerOutput(BaseModel):
    decisions: dict[str, PortfolioDecision]


class PortfolioManagementAgent:
    def __init__(self, api_key: str = None, model_name: str = "gemini"):
        """
        Initializes the Portfolio Management Agent.
        Supports Google Gemini, OpenAI, and Ollama (DeepSeek R1, Llama, etc.).
        """
        self.model_provider = model_name.lower()
        self.api_key = api_key
        self.logger = logging.getLogger(self.__class__.__name__)

        if self.model_provider == "gemini":
            if not genai:
                raise ImportError("Google Gemini module is not installed. Run `pip install google-generativeai`")
            genai.configure(api_key=api_key)
            self.model = genai.GenerativeModel(SUPPORTED_MODELS["gemini"])

        elif self.model_provider == "openai":
            import openai  # Ensure OpenAI is imported only when used
            self.openai_client = openai.ChatCompletion

        elif self.model_provider == "ollama":
            if not ollama:
                raise ImportError("Ollama module is not installed. Run `pip install ollama`")
            self.model = SUPPORTED_MODELS["ollama"]

        else:
            raise ValueError(f"Unsupported model provider: {model_name}. Choose from: {list(SUPPORTED_MODELS.keys())}")

    def make_decision(self, state: AgentState) -> dict:
        """Generates trading decisions using the selected AI model."""
        data = state["data"]
        portfolio = data["portfolio"]
        analyst_signals = data["analyst_signals"]
        tickers = data["tickers"]

        progress.update_status("portfolio_management_agent", None, "Aggregating signals")
        agg_signals = self.aggregate_signals(tickers, analyst_signals)

        progress.update_status("portfolio_management_agent", None, "Preparing prompt")
        prompt_text = self.generate_prompt(agg_signals, portfolio)

        progress.update_status("portfolio_management_agent", None, f"Invoking {self.model_provider.upper()} AI")
        result = self.invoke_llm(prompt_text, tickers)

        # ✅ Convert PortfolioDecision objects to dictionaries before logging
        decision_dict = {t: d.model_dump() for t, d in result.decisions.items()}
        self.logger.info(f"📌 AI-generated Portfolio Decisions:\n{json.dumps(decision_dict, indent=2)}")

        state["data"]["analyst_signals"]["portfolio_management"] = decision_dict

        message = HumanMessage(content=json.dumps(decision_dict), name="portfolio_management")

        if state.get("metadata", {}).get("show_reasoning", False):
            show_agent_reasoning(decision_dict, "Portfolio Management Agent")

        progress.update_status("portfolio_management_agent", None, "Done")
        return {"messages": state["messages"] + [message], "data": state["data"]}

    def aggregate_signals(self, tickers, analyst_signals):
        """Aggregates signals from different agents to determine market sentiment."""
        agg_signals = {}
        for ticker in tickers:
            weighted_score, total_conf = 0.0, 0.0
            ticker_signals = {}

            for agent, signals in analyst_signals.items():
                if agent == "risk_management_agent" or ticker not in signals:
                    continue
                sig, conf = signals[ticker].get("signal", "neutral"), signals[ticker].get("confidence", 0)
                if sig == "bullish":
                    weighted_score += conf
                elif sig == "bearish":
                    weighted_score -= conf
                total_conf += conf
                ticker_signals[agent] = {"signal": sig, "confidence": conf}

            final_score = weighted_score / (total_conf or 1)
            overall, conf = ("bullish", abs(final_score)) if final_score > 20 else (
                "bearish", abs(final_score)) if final_score < -20 else ("neutral", abs(final_score))

            agg_signals[ticker] = {
                "aggregated_signal": overall,
                "aggregated_confidence": round(conf, 2),
                "signals": ticker_signals,
            }
        return agg_signals

    def generate_prompt(self, agg_signals, portfolio):
        """Generates a prompt for the AI model."""
        return f"""
        You are a portfolio manager making trading decisions.
        - 'bullish' means buy, 'bearish' means sell, 'neutral' means hold.
        Given these aggregated signals: {json.dumps(agg_signals, indent=2)}
        Portfolio:
        - Cash: ${portfolio["cash"]:.2f}
        - Positions: {json.dumps(portfolio["positions"], indent=2)}

        Return JSON formatted decisions:
        {{
            "decisions": {{
                "TICKER": {{
                    "action": "buy/sell/hold",
                    "quantity": NUMBER,
                    "confidence": CONFIDENCE_PERCENTAGE,
                    "reasoning": "EXPLANATION"
                }}
            }}
        }}
        """

    def invoke_llm(self, prompt, tickers, max_retries=3):
        """Invokes the selected AI model (Gemini, OpenAI, or Ollama) with retries."""
        for attempt in range(max_retries):
            try:
                self.logger.info(f"📡 Invoking {self.model_provider.upper()} (Attempt {attempt + 1}/{max_retries})")

                if self.model_provider == "gemini":
                    response = self.model.generate_content(prompt).text

                elif self.model_provider == "openai":
                    response = self.openai_client.create(
                        model=SUPPORTED_MODELS["openai"], messages=[{"role": "user", "content": prompt}]
                    )["choices"][0]["message"]["content"]

                elif self.model_provider == "ollama":
                    response = ollama.chat(model=self.model, messages=[{"role": "user", "content": prompt}])["message"][
                        "content"]

                cleaned_response = response.strip().strip("```json").strip("```")
                decision_json = json.loads(cleaned_response)

                return PortfolioManagerOutput(
                    decisions={t: PortfolioDecision(**decision_json["decisions"][t]) for t in tickers}
                )

            except Exception as e:
                self.logger.error(f"❌ Error in LLM call: {e}")
                time.sleep(5)

        self.logger.error("❌ Portfolio Management failed after multiple attempts.")
        return PortfolioManagerOutput(
            decisions={t: PortfolioDecision(action="hold", quantity=0, confidence=0.0, reasoning="LLM Error.") for t in
                       tickers})
