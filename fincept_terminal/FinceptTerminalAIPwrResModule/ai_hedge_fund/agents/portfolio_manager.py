# filename: portfolio_management_agent.py

import json
import logging
import google.generativeai as genai
from pydantic import BaseModel, Field
from typing_extensions import Literal
from langchain_core.messages import HumanMessage
from langchain_core.prompts import ChatPromptTemplate
import time

from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.data.state import AgentState, show_agent_reasoning
from fincept_terminal.FinceptTerminalAIPwrResModule.ai_hedge_fund.utils.progress import progress

# ✅ Logging configuration
logger = logging.getLogger(__name__)


class PortfolioDecision(BaseModel):
    action: Literal["buy", "sell", "hold"]
    quantity: int = Field(description="Number of shares to trade")
    confidence: float = Field(description="Confidence, 0.0 to 100.0")
    reasoning: str = Field(description="Decision rationale")


class PortfolioManagerOutput(BaseModel):
    decisions: dict[str, PortfolioDecision]


class PortfolioManagementAgent:
    def __init__(self, api_key: str, model_name: str = "models/gemini-1.5-flash"):
        """Initializes the Portfolio Management Agent using Google's Gemini API."""
        genai.configure(api_key=api_key)
        self.model = genai.GenerativeModel(model_name)
        self.logger = logging.getLogger(self.__class__.__name__)

    def make_decision(self, state: AgentState) -> dict:
        """Generates trading decisions using Google Generative AI."""
        data = state["data"]
        portfolio = data["portfolio"]
        analyst_signals = data["analyst_signals"]
        tickers = data["tickers"]

        progress.update_status("portfolio_management_agent", None, "Aggregating signals")
        agg_signals = {}
        for ticker in tickers:
            progress.update_status("portfolio_management_agent", ticker, "Collecting signals")
            risk_data = analyst_signals.get("risk_management_agent", {}).get(ticker, {})
            pos_limit = risk_data.get("remaining_position_limit", 0)
            price = risk_data.get("current_price", 0)

            # ✅ Fix max_shares calculation (Ensure it is never negative)
            max_shares = max(0, int(pos_limit / price)) if price > 0 else 0

            ticker_signals = {}
            weighted_score = 0.0
            total_conf = 0.0

            for agent, signals in analyst_signals.items():
                if agent == "risk_management_agent":
                    continue
                if ticker not in signals:
                    continue
                s = signals[ticker]
                sig = s.get("signal", "neutral")
                c = s.get("confidence", 0)
                if sig == "bullish":
                    weighted_score += c
                elif sig == "bearish":
                    weighted_score -= c
                total_conf += c
                ticker_signals[agent] = {"signal": sig, "confidence": c}

            final_score = weighted_score / (total_conf or 1)
            if final_score > 20:
                overall = "bullish"
                conf = abs(final_score)
            elif final_score < -20:
                overall = "bearish"
                conf = abs(final_score)
            else:
                overall = "neutral"
                conf = abs(final_score)

            agg_signals[ticker] = {
                "aggregated_signal": overall,
                "aggregated_confidence": round(conf, 2),
                "signals": ticker_signals,
                "max_shares": max_shares,  # ✅ Corrected max_shares (No negative values)
                "current_price": price,
            }

        progress.update_status("portfolio_management_agent", None, "Preparing prompt")

        prompt_text = f"""
        You are a portfolio manager. Make trading decisions based on aggregated signals:
        - A 'bullish' signal means the stock is expected to rise.
        - A 'bearish' signal means the stock is expected to fall.
        - A 'neutral' signal means no strong movement is expected.

        Given the following aggregated signals:

        {json.dumps(agg_signals, indent=2)}

        Portfolio:
        - Cash: ${portfolio["cash"]:.2f}
        - Positions: {json.dumps(portfolio["positions"], indent=2)}

        Provide a JSON response matching this format:
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

        progress.update_status("portfolio_management_agent", None, "Invoking Gemini AI")

        # ✅ Invoke Gemini AI for decision-making
        result = self._invoke_portfolio_llm(prompt_text, tickers)

        # ✅ Convert PortfolioDecision objects to dictionaries before logging
        decision_dict = {t: d.model_dump() for t, d in result.decisions.items()}
        self.logger.info(f"📌 AI-generated Portfolio Decisions:\n{json.dumps(decision_dict, indent=2)}")

        # ✅ Store AI response in final_state correctly
        state["data"]["analyst_signals"]["portfolio_management"] = decision_dict

        # ✅ Process Gemini AI response
        message = HumanMessage(
            content=json.dumps(decision_dict),
            name="portfolio_management",
        )

        if state.get("metadata", {}).get("show_reasoning", False):
            show_agent_reasoning(decision_dict, "Portfolio Management Agent")

        progress.update_status("portfolio_management_agent", None, "Done")
        return {"messages": state["messages"] + [message], "data": state["data"]}

    def _invoke_portfolio_llm(self, prompt: str, tickers: list, max_retries: int = 3) -> PortfolioManagerOutput:
        """Invokes the Google Gemini AI API to generate portfolio decisions with enhanced error handling."""
        for attempt in range(max_retries):
            try:
                self.logger.info(f"📡 Sending request to Gemini AI (Attempt {attempt + 1}/{max_retries})")
                response = self.model.generate_content(prompt)

                # ✅ Log raw response for debugging
                self.logger.debug(f"📜 Raw Gemini AI Response:\n{response.text}")

                # ✅ Strip markdown-style formatting if present
                cleaned_response = response.text.strip().strip("```json").strip("```")

                # ✅ Validate JSON output before parsing
                try:
                    decision_json = json.loads(cleaned_response)
                except json.JSONDecodeError:
                    self.logger.error(f"❌ Gemini AI returned invalid JSON: {cleaned_response}")
                    raise ValueError("Invalid JSON format received from Gemini AI")

                if "decisions" not in decision_json:
                    self.logger.error(f"❌ Missing 'decisions' key in response: {cleaned_response}")
                    raise ValueError("Missing 'decisions' key in Gemini AI response")

                return PortfolioManagerOutput(
                    decisions={
                        t: PortfolioDecision(
                            action=d["action"],
                            quantity=d["quantity"],
                            confidence=d["confidence"],
                            reasoning=d["reasoning"]
                        ) for t, d in decision_json["decisions"].items()
                    }
                )

            except Exception as e:
                progress.update_status("portfolio_management_agent", None, f"Error - retry {attempt + 1}/{max_retries}")
                self.logger.error(f"❌ Error in Gemini AI call: {e}")

            self.logger.info("🔄 Retrying in 5 seconds...")
            time.sleep(5)

        self.logger.error("❌ Portfolio Management failed after multiple attempts.")
        return PortfolioManagerOutput(
            decisions={
                t: PortfolioDecision(
                    action="hold",
                    quantity=0,
                    confidence=0.0,
                    reasoning="Error in Gemini AI. Default to hold."
                )
                for t in tickers
            }
        )




