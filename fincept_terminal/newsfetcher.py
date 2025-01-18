import os
import requests
from dotenv import load_dotenv

load_dotenv()

NEWS_API_KEY = os.getenv("NEWS_API_KEY")
NEWS_API_URL = "https://newsapi.org/v2/top-headlines"

def fetch_latest_news(country="us", category=None, page_size=5):
    """
    Fetch the latest news articles using the News API.

    :param country: Country code for news (default: 'us').
    :param category: News category (e.g., 'technology', 'business').
    :param page_size: Number of articles to fetch (default: 5).
    :return: List of news articles or error message.
    """
    # Validate API key
    if not NEWS_API_KEY or NEWS_API_KEY.strip() == "":
        return "API key is missing or empty. Please set NEWS_API_KEY in the .env file."

    params = {
        "apiKey": NEWS_API_KEY,
        "country": country,
        "pageSize": page_size,
    }

    if category:
        params["category"] = category

    try:
        response = requests.get(NEWS_API_URL, params=params)
        response.raise_for_status()  # Raise an exception for HTTP errors

        if response.status_code == 401:  # Unauthorized - Invalid API Key
            return "Invalid API key. Please check your NEWS_API_KEY in the .env file."

        articles = response.json().get("articles", [])

        # Format articles for display
        formatted_articles = []
        for article in articles:
            formatted_articles.append({
                "title": article.get("title", "No title"),
                "description": article.get("description", "No description"),
                "url": article.get("url", "#")
            })

        return formatted_articles

    except requests.exceptions.RequestException as e:
        return f"Error fetching news: {e}"



from fincept_terminal.themes import console
from rich.table import Table

def display_latest_news():
    """Fetch and display the latest news articles."""
    console.print("\n[bold cyan]Fetching the latest news...[/bold cyan]")
    news_articles = fetch_latest_news()

    if isinstance(news_articles, str):  # Handle error messages as strings
        console.print(f"[bold red]{news_articles}[/bold red]")
        return

    if not news_articles:  # Handle cases where no articles are returned
        console.print("[bold yellow]No news articles available.[/bold yellow]")
        return

    table = Table(title="Latest News", header_style="bold green")
    table.add_column("Title", justify="left", style="cyan", no_wrap=True)
    table.add_column("Description", justify="left", style="white", no_wrap=False)
    table.add_column("URL", justify="left", style="yellow", no_wrap=True)

    for article in news_articles:
        table.add_row(article["title"], article["description"], article["url"])

    console.print(table)


