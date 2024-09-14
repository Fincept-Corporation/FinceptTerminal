import warnings
from gnews import GNews
from transformers import pipeline
from rich.table import Table
from fincept_terminal.themes import console
from rich.prompt import Prompt

# Suppress warnings
warnings.filterwarnings("ignore")

# Initialize Hugging Face sentiment analysis pipeline
sentiment_analysis = pipeline("sentiment-analysis")

# Initialize GNews with default settings
google_news = GNews(language='en', max_results=25)

# Country code mapping based on GNews documentation
COUNTRY_CODES = {
    'Australia': 'AU', 'Botswana': 'BW', 'Canada': 'CA', 'Ethiopia': 'ET', 'Ghana': 'GH', 'India': 'IN',
    'Indonesia': 'ID',
    'Ireland': 'IE', 'Israel': 'IL', 'Kenya': 'KE', 'Latvia': 'LV', 'Malaysia': 'MY', 'Namibia': 'NA',
    'New Zealand': 'NZ',
    'Nigeria': 'NG', 'Pakistan': 'PK', 'Philippines': 'PH', 'Singapore': 'SG', 'South Africa': 'ZA', 'Tanzania': 'TZ',
    'Uganda': 'UG', 'United Kingdom': 'GB', 'United States': 'US', 'Zimbabwe': 'ZW', 'Czech Republic': 'CZ',
    'Germany': 'DE', 'Austria': 'AT', 'Switzerland': 'CH', 'Argentina': 'AR', 'Chile': 'CL', 'Colombia': 'CO',
    'Cuba': 'CU', 'Mexico': 'MX', 'Peru': 'PE', 'Venezuela': 'VE', 'Belgium': 'BE', 'France': 'FR', 'Morocco': 'MA',
    'Senegal': 'SN', 'Italy': 'IT', 'Lithuania': 'LT', 'Hungary': 'HU', 'Netherlands': 'NL', 'Norway': 'NO',
    'Poland': 'PL', 'Brazil': 'BR', 'Portugal': 'PT', 'Romania': 'RO', 'Slovakia': 'SK', 'Slovenia': 'SI',
    'Sweden': 'SE', 'Vietnam': 'VN', 'Turkey': 'TR', 'Greece': 'GR', 'Bulgaria': 'BG', 'Russia': 'RU', 'Ukraine': 'UA',
    'Serbia': 'RS', 'United Arab Emirates': 'AE', 'Saudi Arabia': 'SA', 'Lebanon': 'LB', 'Egypt': 'EG',
    'Bangladesh': 'BD', 'Thailand': 'TH', 'China': 'CN', 'Taiwan': 'TW', 'Hong Kong': 'HK', 'Japan': 'JP',
    'Republic of Korea': 'KR'
}


def set_gnews_country(country):
    """Set the GNews country dynamically based on user selection."""
    if country == "WORLD":
        google_news.country = None  # No specific country, fetch global news
    else:
        google_news.country = COUNTRY_CODES.get(country, None)


def analyze_sentiment(text):
    """Analyze the sentiment of the provided text using Hugging Face's pre-trained model."""
    result = sentiment_analysis(text[:512])  # Limit to 512 characters for model compatibility
    sentiment = result[0]['label']  # Get sentiment label (POSITIVE, NEGATIVE, NEUTRAL)
    return sentiment


def fetch_and_display_news(country, topic):
    """Fetch and display news articles along with sentiment analysis for a selected country and topic."""
    console.print(f"[bold cyan]Fetching {topic} news for {country}...[/bold cyan]")
    articles = google_news.get_news_by_topic(topic.lower())

    if not articles:
        console.print(f"[bold red]No news articles found for {topic} in {country}[/bold red]")
        return

    # Display articles and perform sentiment analysis
    table = Table(title=f"{topic} News for {country}", title_justify="center", header_style="bold green on #282828")
    table.add_column("Headline", style="cyan", justify="left", no_wrap=True)
    table.add_column("Sentiment", style="magenta", justify="left")

    positive_count = 0
    neutral_count = 0
    negative_count = 0

    # Loop over the articles, perform sentiment analysis, and display the news
    for article in articles:
        headline = article['title']

        # Perform sentiment analysis using Hugging Face transformers
        sentiment = analyze_sentiment(headline)

        if sentiment == "POSITIVE":
            positive_count += 1
        elif sentiment == "NEGATIVE":
            negative_count += 1
        else:
            neutral_count += 1

        table.add_row(headline, sentiment)

    console.print(table)

    # Step 4: Display sentiment analysis as ASCII bars in one line
    display_sentiment_bar(positive_count, neutral_count, negative_count)


def display_sentiment_bar(positive, neutral, negative):
    """Display sentiment analysis as ASCII bar chart in one line."""
    total = positive + neutral + negative
    if total == 0:
        console.print("[bold red]No sentiment data available.[/bold red]")
        return

    # Calculate the percentage for each sentiment
    positive_percent = (positive / total) * 100
    neutral_percent = (neutral / total) * 100
    negative_percent = (negative / total) * 100

    # Display sentiment as ASCII bar
    bar_length = 40  # Length of the bar
    positive_bar = '█' * int((positive_percent / 100) * bar_length)
    neutral_bar = '█' * int((neutral_percent / 100) * bar_length)
    negative_bar = '█' * int((negative_percent / 100) * bar_length)

    # Create labels for each sentiment category
    positive_label = f"Positive: {positive} ({positive_percent:.2f}%)"
    neutral_label = f"Neutral: {neutral} ({neutral_percent:.2f}%)"
    negative_label = f"Negative: {negative} ({negative_percent:.2f}%)"

    # Display the bars in a single line
    console.print(
        f"\n[bold green]{positive_label}[/bold green] {positive_bar}  [bold yellow]{neutral_label}[/bold yellow] {neutral_bar}  [bold red]{negative_label}[/bold red] {negative_bar}")


def show_news_and_sentiment_menu():
    """Display Global News and Sentiment analysis by continent and country."""
    continents = ["Asia", "Europe", "Africa", "North America", "South America", "Oceania", "Middle East", "Main Menu"]

    while True:  # Loop to keep asking the user for news until they choose to exit
        console.print("[bold cyan]GLOBAL NEWS AND SENTIMENT[/bold cyan]\n", style="info")

        for idx, continent in enumerate(continents, start=1):
            console.print(f"{idx}. {continent}")

        continent_choice = Prompt.ask("Enter your continent choice (Press enter to default to WORLD)", default="WORLD")

        if continent_choice.lower() == "main menu":
            return

        selected_continent = continent_choice

        if selected_continent == "WORLD":
            selected_country = "WORLD"
            set_gnews_country(selected_country)
            fetch_and_display_news(selected_country, "BUSINESS")  # Fetch global news directly without category
        else:
            countries = COUNTRY_CODES.keys()  # For simplicity, we directly use COUNTRY_CODES for this example

            console.print(f"\nSelect a Country in {selected_continent} (or press Enter for WORLD)")
            for idx, country in enumerate(countries, start=1):
                console.print(f"{idx}. {country}")

            country_choice = Prompt.ask("Enter your country choice or press Enter for WORLD", default="WORLD")

            selected_country = country_choice if country_choice in COUNTRY_CODES else "WORLD"
            set_gnews_country(selected_country)

            # Step 3: Prompt for news category (NATION, BUSINESS, TECHNOLOGY)
            topics = ["NATION", "BUSINESS", "TECHNOLOGY"]
            console.print("\nSelect a Topic:")
            for idx, topic in enumerate(topics, start=1):
                console.print(f"{idx}. {topic}")
            topic_choice = Prompt.ask("Enter your topic choice", default="BUSINESS")

            selected_topic = topics[int(topic_choice) - 1] if topic_choice.isdigit() else "BUSINESS"

            fetch_and_display_news(selected_country, selected_topic)

        another_news = Prompt.ask("\nWould you like to fetch more news? (yes/no)").lower()
        if another_news == "no":
            break

