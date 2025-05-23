import logging
import json
import time
import os

# Conditional imports based on the selected model provider
try:
    import google.generativeai as genai  # Google Gemini
except ImportError:
    genai = None

try:
    import ollama  # Ollama LLMs (DeepSeek R1, Llama, etc.)
except ImportError:
    ollama = None

class SentimentAgent:
    def __init__(self, api_key: str = None, model_name: str = "gemini"):
        """
        Initializes the Sentiment Analysis Agent.
        Supports Google Gemini, OpenAI, and Ollama (DeepSeek R1, Llama, etc.).
        """
        self.model_provider = model_name.lower()
        self.api_key = api_key
        self.logger = logging.getLogger(self.__class__.__name__)

        SUPPORTED_MODELS = {
            "gemini": "models/gemini-1.5-flash",
            "openai": "gpt-4-turbo",
            "ollama": "deepseek"
        }

        self.model = SUPPORTED_MODELS.get(self.model_provider, "gemini")

        if self.model_provider == "gemini":
            if not genai:
                raise ImportError("Google Gemini module is not installed. Run `pip install google-generativeai`")
            genai.configure(api_key=api_key)
            self.model = genai.GenerativeModel(self.model)

        elif self.model_provider == "openai":
            import openai  # Ensure OpenAI is imported only when used
            self.openai_client = openai.ChatCompletion

        elif self.model_provider == "ollama":
            if not ollama:
                raise ImportError("Ollama module is not installed. Run `pip install ollama`")
        else:
            raise ValueError(f"Unsupported model provider: {model_name}. Choose from: {list(SUPPORTED_MODELS.keys())}")

    def analyze_sentiment(self, text: str) -> dict:
        """Analyzes sentiment of financial news using the selected AI model."""
        self.logger.info("🔍 Analyzing sentiment for text: %s", text)

        prompt = (
            "You are an expert financial sentiment analyzer. "
            "Classify the sentiment (Positive, Negative, Neutral) of the text and provide a concise rationale.\n\n"
            f"Text: {text}\n"
            "Return JSON: { 'sentiment': 'Positive/Negative/Neutral', 'rationale': 'Brief Explanation' }"
        )

        self.logger.info(f"📡 Sending prompt to {self.model_provider.upper()} AI")

        # ✅ Call the AI model
        result = self.invoke_llm(prompt)

        sentiment = result.get("sentiment", "Neutral")
        rationale = result.get("rationale", "No rationale provided.")

        self.logger.info(f"✅ Sentiment: {sentiment}, Rationale: {rationale}")

        return {"sentiment": sentiment, "rationale": rationale}

    def invoke_llm(self, prompt: str, max_retries: int = 3) -> dict:
        """Invokes the selected AI model (Gemini, OpenAI, or Ollama) with retries."""
        for attempt in range(max_retries):
            try:
                self.logger.info(f"📡 Invoking {self.model_provider.upper()} (Attempt {attempt + 1}/{max_retries})")

                if self.model_provider == "gemini":
                    response = self.model.generate_content(prompt).text

                elif self.model_provider == "openai":
                    response = self.openai_client.create(
                        model="gpt-4-turbo",
                        messages=[{"role": "user", "content": prompt}]
                    )["choices"][0]["message"]["content"]

                elif self.model_provider == "ollama":
                    response = ollama.chat(model=self.model, messages=[{"role": "user", "content": prompt}])["message"]["content"]

                # ✅ Clean and parse JSON response
                cleaned_response = response.strip().strip("```json").strip("```")
                return json.loads(cleaned_response)

            except Exception as e:
                self.logger.error(f"❌ Error in LLM call: {e}")
                time.sleep(5)

        self.logger.error("❌ Sentiment Analysis failed after multiple attempts.")
        return {"sentiment": "Neutral", "rationale": "LLM Error. Defaulting to neutral."}
