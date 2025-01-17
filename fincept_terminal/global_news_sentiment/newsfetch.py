import warnings
from gnews import GNews
from transformers import pipeline
from fincept_terminal.utils.themes import console
from fincept_terminal.utils.const import display_in_columns
from rich.prompt import Prompt

# Suppress warnings from transformers
warnings.filterwarnings("ignore", category=UserWarning)

# Initialize Hugging Face sentiment analysis pipeline
sentiment_analysis = pipeline("sentiment-analysis")

# Initialize GNews with default settings
google_news = GNews(language='en', max_results=25)

# Country code mapping based on GNews documentation
COUNTRY_CODES = {
    'Australia': 'AU', 'Botswana': 'BW', 'Canada': 'CA', 'Ethiopia': 'ET', 'Ghana': 'GH', 'India': 'IN',
    'Indonesia': 'ID', 'Ireland': 'IE', 'Israel': 'IL', 'Kenya': 'KE', 'Latvia': 'LV', 'Malaysia': 'MY',
    'Namibia': 'NA', 'New Zealand': 'NZ', 'Nigeria': 'NG', 'Pakistan': 'PK', 'Philippines': 'PH',
    'Singapore': 'SG', 'South Africa': 'ZA', 'Tanzania': 'TZ', 'Uganda': 'UG', 'United Kingdom': 'GB',
    'United States': 'US', 'Zimbabwe': 'ZW', 'Czech Republic': 'CZ', 'Germany': 'DE', 'Austria': 'AT',
    'Switzerland': 'CH', 'Argentina': 'AR', 'Chile': 'CL', 'Colombia': 'CO', 'Cuba': 'CU', 'Mexico': 'MX',
    'Peru': 'PE', 'Venezuela': 'VE', 'Belgium': 'BE', 'France': 'FR', 'Morocco': 'MA', 'Senegal': 'SN',
    'Italy': 'IT', 'Lithuania': 'LT', 'Hungary': 'HU', 'Netherlands': 'NL', 'Norway': 'NO', 'Poland': 'PL',
    'Brazil': 'BR', 'Portugal': 'PT', 'Romania': 'RO', 'Slovakia': 'SK', 'Slovenia': 'SI', 'Sweden': 'SE',
    'Vietnam': 'VN', 'Turkey': 'TR', 'Greece': 'GR', 'Bulgaria': 'BG', 'Russia': 'RU', 'Ukraine': 'UA',
    'Serbia': 'RS', 'United Arab Emirates': 'AE', 'Saudi Arabia': 'SA', 'Lebanon': 'LB', 'Egypt': 'EG',
    'Bangladesh': 'BD', 'Thailand': 'TH', 'China': 'CN', 'Taiwan': 'TW', 'Hong Kong': 'HK', 'Japan': 'JP',
    'Republic of Korea': 'KR'
}

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

def set_gnews_country(country):
    """Set the GNews country dynamically based on user selection."""
    google_news.country = COUNTRY_CODES.get(country, None)

def analyze_sentiment(text):
    """Analyze the sentiment of the provided text using Hugging Face's pre-trained model."""
    result = sentiment_analysis(text[:512])  # Limit to 512 characters for model compatibility
    sentiment = result[0]['label']  # Get sentiment label (POSITIVE, NEGATIVE, NEUTRAL)
    return sentiment

def fetch_and_display_news(country, topic):
    """Fetch and display news articles along with sentiment analysis for a selected country and topic."""
    console.print(f"[bold cyan]Fetching {topic} news for {country}...[/bold cyan]")
    set_gnews_country(country)
    articles = google_news.get_news_by_topic(topic.lower())  # Fetch news based on the topic and country

    if not articles:
        console.print(f"[bold red]No news articles found for {topic} in {country}[/bold red]")
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

def fetch_custom_news(query, country):
    """Fetch and display news articles based on a custom keyword query."""
    console.print(f"[bold cyan]Fetching news for query: {query} in {country}...[/bold cyan]")
    set_gnews_country(country)
    articles = google_news.get_news(query)

    if not articles:
        console.print(f"[bold red]No news articles found for query: {query} in {country}[/bold red]")
        return

    # Display articles and perform sentiment analysis
    from rich.table import Table
    table = Table(title=f"News for '{query}' in {country}", title_justify="center", header_style="bold green on #282828")
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

    positive_label = f"Positive: {positive} ({positive_percent:.2f}%)"
    neutral_label = f"Neutral: {neutral} ({neutral_percent:.2f}%)"
    negative_label = f"Negative: {negative} ({negative_percent:.2f}%)"

    console.print(
        f"\n[bold green]{positive_label}[/bold green] {positive_bar}  [bold yellow]{neutral_label}[/bold yellow] {neutral_bar}  [bold red]{negative_label}[/bold red] {negative_bar}")

def show_news_and_sentiment_menu():
    """Display Global News and Sentiment analysis by continent and country."""
    continents = list(CONTINENT_COUNTRIES.keys()) + ["WORLD", "BACK TO MAIN MENU"]

    while True:
        console.print("[bold cyan]GLOBAL NEWS AND SENTIMENT[/bold cyan]\n", style="info")
        display_in_columns("Select a Continent or WORLD", continents)

        continent_choice = Prompt.ask("Enter your continent choice (Press enter to default to WORLD)", default="WORLD")

        if continent_choice == "BACK TO MAIN MENU":
            return

        try:
            selected_continent = continents[int(continent_choice) - 1] if continent_choice.isdigit() else continent_choice
        except (IndexError, ValueError):
            console.print(f"[bold red]Invalid choice. Please select a valid continent or WORLD.[/bold red]")
            continue

        if selected_continent == "WORLD":
            selected_country = "WORLD"
            google_news.country = None
            console.print(f"[bold cyan]Fetching global news...[/bold cyan]")
            fetch_custom_news("Business", selected_country)
        elif selected_continent == "BACK TO MAIN MENU":
            from fincept_terminal.cli import show_main_menu
            show_main_menu()
        else:
            countries = CONTINENT_COUNTRIES[selected_continent] + ["BACK TO MAIN MENU"]

            display_in_columns(f"Select a Country in {selected_continent}", countries)

            country_choice = Prompt.ask("Enter your country choice", default="WORLD")
            selected_country = countries[int(country_choice) - 1] if country_choice.isdigit() else country_choice

            if selected_country == "BACK TO MAIN MENU":
                from fincept_terminal.cli import show_main_menu
                show_main_menu()
            elif selected_country not in countries:
                console.print(f"[bold red]Invalid country selection. Please choose a valid country from the list.[/bold red]")
                continue

            set_gnews_country(selected_country)

            topics = ["NATION", "BUSINESS", "TECHNOLOGY", "CUSTOM KEYWORD"]
            topics.append("BACK TO MAIN MENU")
            display_in_columns("Select a Topic", topics)

            topic_choice = Prompt.ask("Enter your topic choice", default="BUSINESS")
            # Ensure valid input before proceeding
            if topic_choice.isdigit():
                topic_choice = int(topic_choice)
                if topic_choice < 1 or topic_choice > len(topics):
                    console.print("[bold red]Invalid choice. Please try again.[/bold red]")
                    return
            else:
                console.print("[bold red]Invalid choice. Please enter a valid number.[/bold red]")
                return

            selected_topic = topics[topic_choice - 1]

            if selected_topic == "BACK TO MAIN MENU":
                from fincept_terminal.cli import show_main_menu
                show_main_menu()
            elif selected_topic == "CUSTOM KEYWORD":
                keyword = Prompt.ask("Enter the keyword you want to search for (e.g., HDFC Bank)")
                fetch_custom_news(keyword, selected_country)
            else:
                fetch_and_display_news(selected_country, selected_topic)

        another_news = Prompt.ask("\nWould you like to fetch more news? (yes/no)").lower()
        if another_news == "no":
            from fincept_terminal.cli import show_main_menu
            show_main_menu()
            break
