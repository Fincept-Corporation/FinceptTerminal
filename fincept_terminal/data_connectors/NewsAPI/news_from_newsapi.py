from fincept_terminal.utils.themes import console
import requests


def fetch_newsapi_news(topic, country, api_key):
    console.print(f"[bold cyan]Fetching {topic} news from NewsAPI for {country}...[/bold cyan]")
    url = f"https://newsapi.org/v2/everything?q={topic}&language=en&pageSize=10"
    headers = {"Authorization": f"Bearer {api_key}"}

    response = requests.get(url, headers=headers)
    if response.status_code != 200:
        console.print(f"[bold red]Failed to fetch news from NewsAPI: {response.status_code}[/bold red]")
        return []

    data = response.json()
    if "articles" in data:
        return [{"title": article["title"]} for article in data["articles"]]
    else:
        console.print(f"[bold red]No news articles found for {topic} in {country} using NewsAPI.[/bold red]")
        return []
