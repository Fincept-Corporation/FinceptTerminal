import logging
import google.generativeai as genai

class SentimentAgent:
    def __init__(self, api_key: str, model_name: str = "models/gemini-1.5-flash"):
        genai.configure(api_key=api_key)
        self.model = genai.GenerativeModel(model_name)
        self.logger = logging.getLogger(self.__class__.__name__)

    def analyze_sentiment(self, text: str) -> dict:
        self.logger.info("Analyzing sentiment for text: %s", text)
        prompt = (
            "You are an expert finance sentiment analyzer. "
            "Classify the sentiment (Positive, Negative, Neutral) of the text, "
            "and provide a concise rationale.\n\n"
            f"Text: {text}\n"
            "Sentiment:"
        )
        response = self.model.generate_content(prompt)
        lines = response.text.strip().split("\n")
        sentiment = lines[0] if lines else "Neutral"
        rationale = lines[1] if len(lines) > 1 else ""
        self.logger.info("Sentiment result: %s, Rationale: %s", sentiment, rationale)
        return {"sentiment": sentiment, "rationale": rationale}
