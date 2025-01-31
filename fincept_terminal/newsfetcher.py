import warnings
from gnews import GNews
from transformers import pipeline
from fincept_terminal.utils.themes import console
from fincept_terminal.utils.const import display_in_columns
from rich.prompt import Prompt
import ujson as json
import os

# Suppress warnings from transformers
warnings.filterwarnings("ignore", category=UserWarning)

# Initialize Hugging Face sentiment analysis pipeline
sentiment_analysis = pipeline("sentiment-analysis")

# Initialize GNews with default settings
google_news = GNews(language='en', max_results=25)

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
SETTINGS_FILE = os.path.join(BASE_DIR, "settings.json")

# Mapping of continents to the countries they contain
CONTINENT_COUNTRIES = {
    'ASIA': ['INDIA', 'INDONESIA', 'ISRAEL', 'JAPAN', 'REPUBLIC OF KOREA', 'CHINA', 'TAIWAN', 'THAILAND', 'VIETNAM'],
    'EUROPE': ['UNITED KINGDOM', 'GERMANY', 'FRANCE', 'ITALY', 'SPAIN', 'NETHERLANDS', 'SWEDEN', 'NORWAY'],
    'AFRICA': ['SOUTH AFRICA', 'NIGERIA', 'KENYA', 'ETHIOPIA', 'GHANA', 'UGANDA', 'TANZANIA'],
    'NORTH AMERICA': ['UNITED STATES', 'CANADA', 'MEXICO'],
    'SOUTH AMERICA': ['BRAZIL', 'ARGENTINA', 'COLOMBIA', 'CHILE'],
    'OCEANIA': ['AUSTRALIA', 'NEW ZEALAND'],
    'MIDDLE EAST': ['UNITED ARAB EMIRATES', 'SAUDI ARABIA', 'LEBANON', 'EGYPT']
}

# Helper: Load settings
def load_settings():
    if os.path.exists(SETTINGS_FILE):
        with open(SETTINGS_FILE, "r") as f:
            return json.load(f)
    return {}

# Helper: Analyze sentiment
def analyze_sentiment(text):
    result = sentiment_analysis(text[:512])  # Limit to 512 characters for model compatibility
    sentiment = result[0]['label']  # Get sentiment label (POSITIVE, NEGATIVE, NEUTRAL)
    return sentiment

# Fetch and display news with sentiment analysis
def fetch_and_display_news_with_sentiment(topic, country):
    from fincept_terminal.data_connectors.Gnews.news_from_gnews import fetch_gnews_news
    from fincept_terminal.data_connectors.NewsAPI.news_from_newsapi import fetch_newsapi_news
    from fincept_terminal.data_connectors.RSS_Feed.news_from_rssfeed import fetch_rss_feed_news

    settings = load_settings()
    source = settings.get("data_sources", {}).get("Global News & Sentiment", {}).get("source")
    api_key = settings.get("data_sources", {}).get("Global News & Sentiment", {}).get("api_key")
    rss_feed_urls = settings.get("data_sources", {}).get("Global News & Sentiment", {}).get("rss_feeds")

    # Determine the source to use
    if source == "GNews":
        articles = fetch_gnews_news(topic, country)
    elif source == "News API":
        articles = fetch_newsapi_news(topic, country, api_key)
    elif source == "RSS_Feed (API)":
        articles = []
        for rss_feed_url in rss_feed_urls:
            articles.extend(fetch_rss_feed_news(rss_feed_url))
    else:
        console.print("[bold red]No valid source configured for Global News & Sentiment.[/bold red]")
        return

    # If no articles are found
    if not articles:
        console.print("[bold red]No articles found.[/bold red]")
        return

    # Display articles and perform sentiment analysis
    from rich.table import Table
    table = Table(title=f"{topic} News for {country}", title_justify="center", header_style="bold green on #282828")
    table.add_column("Headline", style="cyan", justify="left", no_wrap=False)
    table.add_column("Sentiment", style="magenta", justify="center")

    positive_count = 0
    neutral_count = 0
    negative_count = 0

    for article in articles:
        headline = article['title']
        sentiment = analyze_sentiment(headline)

        if sentiment == "POSITIVE":
            positive_count += 1
        elif sentiment == "NEGATIVE":
            negative_count += 1
        else:
            neutral_count += 1

        table.add_row(headline, sentiment)

    console.print(table)
    display_sentiment_bar(positive_count, neutral_count, negative_count)
# Display sentiment bar
def display_sentiment_bar(positive, neutral, negative):
    total = positive + neutral + negative
    if total == 0:
        console.print("[bold red]No sentiment data available.[/bold red]")
        return

    positive_percent = (positive / total) * 100
    neutral_percent = (neutral / total) * 100
    negative_percent = (negative / total) * 100

    bar_length = 40
    positive_bar = '█' * int((positive_percent / 100) * bar_length)
    neutral_bar = '█' * int((neutral_percent / 100) * bar_length)
    negative_bar = '█' * int((negative_percent / 100) * bar_length)

    positive_label = f"Positive: {positive} ({positive_percent:.2f}%)"
    neutral_label = f"Neutral: {neutral} ({neutral_percent:.2f}%)"
    negative_label = f"Negative: {negative} ({negative_percent:.2f}%)"

    console.print(
        f"\n[bold green]{positive_label}[/bold green] {positive_bar}  "
        f"[bold yellow]{neutral_label}[/bold yellow] {neutral_bar}  "
        f"[bold red]{negative_label}[/bold red] {negative_bar}"
    )
def show_news_and_sentiment_menu():
    from fincept_terminal.data_connectors.Gnews.news_from_gnews import set_gnews_country

    settings = load_settings()
    news_source = settings.get("data_sources", {}).get("Global News & Sentiment", {}).get("source", "GNews")
    api_key = settings.get("data_sources", {}).get("Global News & Sentiment", {}).get("api_key")
    rss_feed_url = settings.get("data_sources", {}).get("Global News & Sentiment", {}).get("rss_feeds")

    continents = list(CONTINENT_COUNTRIES.keys()) + ["WORLD", "BACK TO MAIN MENU"]

    while True:
        console.print("[bold cyan]GLOBAL NEWS AND SENTIMENT[/bold cyan]\n", style="info")
        display_in_columns("Select a Continent or WORLD", continents)

        continent_choice = Prompt.ask("Enter your continent choice (Press enter to default to WORLD)", default="WORLD")

        if continent_choice.upper() == "BACK TO MAIN MENU":
            return

        try:
            selected_continent = continents[int(continent_choice) - 1] if continent_choice.isdigit() else continent_choice
        except (IndexError, ValueError):
            console.print(f"[bold red]Invalid choice. Please select a valid continent or WORLD.[/bold red]")
            continue

        if selected_continent.upper() == "WORLD":
            selected_country = "WORLD"
            google_news.country = None
            console.print(f"[bold cyan]Fetching global news...[/bold cyan]")
        else:
            countries = CONTINENT_COUNTRIES[selected_continent.upper()] + ["BACK TO MAIN MENU"]
            display_in_columns(f"Select a Country in {selected_continent}", countries)

            country_choice = Prompt.ask("Enter your country choice", default="WORLD")
            try:
                selected_country = countries[int(country_choice) - 1] if country_choice.isdigit() else country_choice
            except (IndexError, ValueError):
                console.print(f"[bold red]Invalid country selection. Please choose a valid country from the list.[/bold red]")
                continue

            if selected_country.upper() == "BACK TO MAIN MENU":
                return

            set_gnews_country(selected_country)

        # Topic selection
        topics = ["NATION", "BUSINESS", "TECHNOLOGY", "CUSTOM KEYWORD", "BACK TO MAIN MENU"]
        display_in_columns("Select a Topic", topics)

        topic_choice = Prompt.ask("Enter your topic choice", default="BUSINESS")
        try:
            selected_topic = topics[int(topic_choice) - 1] if topic_choice.isdigit() else topic_choice
        except (IndexError, ValueError):
            console.print("[bold red]Invalid choice. Please select a valid topic.[/bold red]")
            continue

        if selected_topic.upper() == "BACK TO MAIN MENU":
            return

        if selected_topic.upper() == "CUSTOM KEYWORD":
            keyword = Prompt.ask("Enter the keyword you want to search for (e.g., HDFC Bank)")
            selected_topic = keyword

        # Fetch and display news based on the selected source
        if news_source == "GNews":
            fetch_and_display_news_with_sentiment(selected_topic, selected_country)
        elif news_source == "News API" and api_key:
            fetch_and_display_news_with_sentiment(selected_topic, selected_country)
        elif news_source == "RSS_Feed (API)" and rss_feed_url:
            fetch_and_display_news_with_sentiment(selected_topic, selected_country)
        else:
            console.print("[bold red]No valid news source configured. Please check your settings.[/bold red]")
            return

        another_news = Prompt.ask("\nWould you like to fetch more news? (yes/no)", default="no").lower()
        if another_news != "yes":
            console.print("\n[bold yellow]Returning to the main menu...[/bold yellow]")
            from fincept_terminal.oldTerminal.cli import show_main_menu
            return show_main_menu()

