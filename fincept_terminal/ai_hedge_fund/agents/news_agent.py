import logging
import google.generativeai as genai

class NewsAgent:
    def __init__(self, api_key: str, model_name: str = "gemini-1.5-turbo"):
        genai.configure(api_key=api_key)
        self.model = genai.GenerativeModel(model_name)
        self.logger = logging.getLogger(self.__class__.__name__)

    def summarize_news(self, ticker: str) -> dict:
        self.logger.info("Summarizing news for %s", ticker)
        # In practice, fetch articles from an API, then combine them into a prompt
        sample_articles = (
            f"Headline 1 about {ticker}. "
            f"Headline 2 about {ticker}. "
            f"Headline 3 about {ticker}."
        )
        prompt = (
            "Read the following headlines and provide a concise summary:\n"
            + sample_articles
        )
        response = self.model.generate_content(prompt)
        return {"summary": response.text.strip()}
